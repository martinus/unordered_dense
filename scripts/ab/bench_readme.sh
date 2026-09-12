#!/bin/bash
# The README's two benchmark graphs, end to end: build every map's own binary, run the five
# workloads across an octave, write the CSV and draw the SVGs.
#
#   scripts/ab/bench_readme.sh [-c COMPILER] [-b BASE] [-r ROUNDS] [-o OUTDIR] [-w WORKLOAD,...] [map...]
#
# -w re-measures a subset and updates those rows of the CSV in place, leaving the rest as they
# were -- the five workloads are independent processes and nothing in one reaches the others, so
# a panel can be re-taken without paying for the four beside it. Only do that within one sitting:
# rows measured on different days are rows from different machines.
#
# The default base is a million entries. Not because it is the commonest size -- 32000 is, and the
# blog post's charts use it -- but because it is the smallest one at which every row of this chart
# means something: the huge page allocator does nothing until a block reaches 2 MB, which the values
# do at 131072 entries and the index at about 370000, and a 16 MB segment holds 32000 entries ten
# times over. A chart that draws those three variants at 32000 draws them doing nothing, or paying
# for a segment they do not fill: measured, the segmented huge-page map reads 447 bytes per entry
# there against the plain map's 41.
#
# Every map is in the configuration a caller gets by typing its type name -- its own hash, its own
# defaults -- which is what a README graph is about and the one thing this differs in from maps.sh
# next door, whose question is which index is faster and which therefore hands them all one hash.
#
# One binary per map, for the reason scripts/ab/window.cpp gives at the top: a binary holding a
# dozen maps has a code layout that moves by more than the differences being drawn.
#
# The include paths are maps.sh's and are documented there. AB_CORE pins the measured process.
set -euo pipefail
export LC_ALL=C
cxx=clang++ base=1000000 rounds=3 out=doc works=build,buildfree,find,churn,iterate,memory,rss
while getopts "c:b:r:o:w:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        b) base=$OPTARG ;;
        r) rounds=$OPTARG ;;
        o) out=$OPTARG ;;
        w) works=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
want=("$@")
IFS=, read -r -a work_list <<<"$works"

root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build" "$out"
nb=${NANOBENCH_INCLUDE:-$root/test}

# The release this index replaced, in its own namespace beside the current header.
rename() { # <rev> <namespace> <MACRO> <out>
    git -C "$root" show "$1:include/ankerl/unordered_dense.h" |
        sed "s/ankerl::unordered_dense/$2::unordered_dense/g; s/ANKERL_UNORDERED_DENSE/$3/g; s/namespace ankerl/namespace $2/g; s|#        include \"stl.h\"|#        include <ankerl/stl.h>|" > "$4"
}
rename v4.11.0 udmbase UDMBASE_UNORDERED_DENSE "$build/base411.h"

flags=(-O3 -DNDEBUG -std=c++20 -w -DUDM_DEFAULT_HASH -DUDM_VARIANTS -I"$build" -I"$nb" -I"$root/include" -I"$root/test")
libs=(-ldl) srcs=()
try() { local name=$1 header=$2; shift 2
    if printf '#include <%s>\nint main() {}\n' "$header" | "$cxx" -x c++ -std=c++20 -fsyntax-only "$@" - 2>/dev/null; then
        flags+=("$@"); return 0
    fi
    echo "missing: $name" >&2; return 1
}
absl=${ABSL_ROOT:-/home/martinus/gra/abseil-install}
if try absl absl/container/flat_hash_map.h -I"$absl/include"; then
    libs+=(-Wl,--start-group "$absl"/lib64/libabsl_*.a -Wl,--end-group)
fi
folly=${FOLLY_ROOT:-/home/martinus/gra/folly}
follycfg=${FOLLY_CONFIG:-/home/martinus/gra/folly-config}
if try f14 folly/container/F14Map.h -I"$folly" -I"$follycfg"; then
    srcs+=("$folly/folly/container/detail/F14Table.cpp" "$folly/folly/lang/SafeAssert.cpp" "$folly/folly/lang/ToAscii.cpp")
fi
try emhash emhash/hash_table8.hpp -I"${EMHASH_INCLUDE:-/home/martinus/gra/emhash/include}" || true
try indivi indivi/flat_umap.h -I"${INDIVI_INCLUDE:-/home/martinus/gra/indivi_collection/calmsand/src}" || true

nbo="$build/nanobench_$(basename "$cxx").o"
[ -f "$nbo" ] || (cd "$build" && printf '#define ANKERL_NANOBENCH_IMPLEMENT\n#include <third-party/nanobench.h>\n' > nb.cpp && "$cxx" -O2 -DNDEBUG -std=c++20 -w -I"$nb" -c nb.cpp -o "$nbo")

# maps.cpp knows the list; ask it rather than keeping a second copy of the order here. Under its own
# name, not $build/maps: maps.sh builds that path from the same source without -DUDM_DEFAULT_HASH or
# -DUDM_VARIANTS, so the two binaries hold different lists in a different order, and maps_one.sh
# reads whichever is there to label its rows. Sharing one AB_BUILD would mislabel them silently.
"$cxx" "${flags[@]}" "$root/scripts/ab/maps.cpp" "$nbo" "${srcs[@]}" "${libs[@]}" -o "$build/maps_readme"
csv="$out/bench_readme.csv"
raw="$build/raw.txt"
: > "$raw"

# Build every binary first, then measure -- so that no compile lands between two maps' cells.
declare -A bins
for keys in u64 str; do
    mapfile -t names < <("$build/maps_readme" names "$keys" 2>/dev/null | tr ' ' '\n' | grep -v '^$')
    [ ${#names[@]} -gt 0 ] || { echo "cannot ask maps for its $keys names" >&2; exit 1; }
    kflag=(); [ "$keys" = str ] && kflag=(-DUDM_ONE_STR)
    sel=()
    for i in "${!names[@]}"; do
        name=${names[$i]}
        if [ ${#want[@]} -gt 0 ]; then
            found=0; for w in "${want[@]}"; do [ "$w" = "$name" ] && found=1; done
            [ $found = 1 ] || continue
        fi
        slug=${name//[^a-zA-Z0-9]/_}
        # Two binaries per map: the timed one, and the one that counts bytes. Interposing malloc
        # costs an allocation-heavy map a few percent and a dense one nothing, so the four timed
        # workloads must not be measured in a process that has the interposers linked in at all.
        for kind in one mem; do
            [ "$kind" = mem ] && [[ ,$works, == *,memory,* ]] || [ "$kind" = one ] || continue
            [ "$kind" = one ] && [[ ,$works, == *,build,* || ,$works, == *,find,* || ,$works, == *,churn,* || ,$works, == *,iterate,* || ,$works, == *,rss,* || ,$works, == *,buildfree,* ]] || [ "$kind" = mem ] || continue
            bin="$build/${kind}_${keys}_${slug}"
            cflag=(); [ "$kind" = mem ] && cflag=(-DUDM_COUNT_ALLOC)
            # Not just -f: a binary left in AB_BUILD by an earlier run is the one thing here that
            # would report a number from code no longer in the tree and say nothing about it. The
            # map's own headers are in the list too -- a stale *subject* is worse than a stale
            # harness, and the first version of this check looked only at the harness.
            if [ ! -f "$bin" ] || [ -n "$(find "$root/include" "$root/scripts/ab/bench_readme.cpp" "$root/scripts/ab/maps.h" -newer "$bin" -print -quit)" ]; then
                "$cxx" "${flags[@]}" "${kflag[@]}" "${cflag[@]}" -DUDM_ONE_MAP="$i" \
                    "$root/scripts/ab/bench_readme.cpp" "$nbo" "${srcs[@]}" "${libs[@]}" -o "$bin"
            fi
            bins[$kind/$keys/$name]=$bin
        done
        sel+=("$name")
    done
    echo "== $keys: ${sel[*]}" >&2
    printf '%s\n' "${sel[@]}" > "$build/names_$keys"
done

# Rounds outermost and the maps inside them, so that a machine which drifts across the run drifts
# across every map rather than across whichever one happened to be measured late. The median over
# rounds is what the CSV gets: a phase-separated loop put 6-13% of drift straight into the ratios.
for r in $(seq 1 "$rounds"); do
    for keys in u64 str; do
        mapfile -t sel < "$build/names_$keys"
        for work in "${work_list[@]}"; do
            for name in "${sel[@]}"; do
                kind=one; [ "$work" = memory ] && kind=mem
                line=$(${AB_CORE:+taskset -c $AB_CORE} "${bins[$kind/$keys/$name]}" "$work" "$base")
                printf '%s %s %s %s\n' "$keys" "$work" "$name" "$(awk '{print $NF}' <<<"$line")" >> "$raw"
                echo "  r$r $line" >&2
            done
        done
    done
done

python3 - "$raw" "$csv" "$base" <<'MEDIAN_EOF'
import csv, os, statistics, sys
from collections import defaultdict
vals = defaultdict(list)
for line in open(sys.argv[1]):
    keys, work, name, v = line.split()
    vals[(keys, work, name)].append(float(v))
# Rows measured now replace their old selves; rows not measured now are carried over, which is what
# makes -w able to re-take one panel. Order is whatever the file had, then anything new appended.
base = sys.argv[3]
rows = list(csv.reader(open(sys.argv[2]))) if os.path.exists(sys.argv[2]) else []
fresh = {(k, w, n): f"{statistics.median(xs):.4f}" for (k, w, n), xs in vals.items()}
out = []
for r in rows:
    if len(r) == 6 and r[2] == base and (r[0], r[1], r[3]) in fresh:
        v = fresh.pop((r[0], r[1], r[3]))
        r = [r[0], r[1], r[2], r[3], v, v]
    out.append(r)
for (k, w, n), v in fresh.items():
    out.append([k, w, base, n, v, v])
# Column 6 is the ratio to this map in the same cell. Keyed by the base as well as the key and the
# workload: the file is allowed to hold more than one size, mapsplot filters on it, and a ratio that
# ignored it would divide every row by whichever size was written last.
ref = {(r[0], r[1], r[2]): float(r[4]) for r in out if r[3] == "udm"}
for r in out:
    denom = ref.get((r[0], r[1], r[2]))
    if denom:
        r[5] = f"{float(r[4]) / denom:.4f}"
with open(sys.argv[2], "w", newline="") as f:
    csv.writer(f).writerows(out)
MEDIAN_EOF

# Two charts per key type. The first is this map and the release it replaced against the other
# libraries; the second is the same numbers for the shapes this map can be asked to take, which are
# a configuration question rather than a library one and would otherwise take three of the rows a
# reader is comparing libraries in.
udm_rows=udm,udm-segmented,udm-huge,udm-seg-huge
panels=(buildfree:"build + destroy" find:"find, 50% hits" churn iterate rss:"peak memory")
for keys in u64 str; do
    python3 "$root/scripts/ab/mapsplot.py" readme "$csv" "$keys" "$base" "$out/bench-readme-$keys.svg" \
        --drop="${udm_rows#udm,}" "${panels[@]}"
    python3 "$root/scripts/ab/mapsplot.py" readme "$csv" "$keys" "$base" "$out/bench-readme-udm-$keys.svg" \
        --only="$udm_rows" --title="what this map itself can be asked to be" "${panels[@]}"
done
echo "wrote $csv and $out/bench-readme{,-udm}-{u64,str}.svg" >&2
