#!/bin/bash
# Variants of the header that change what a churned table does about its drift, one map per binary:
# a baseline revision's header against the working tree's header with patches applied to a copy.
#
#   AB_BUILD=/home/martinus/gra/x scripts/ab/drift_variants.sh <rounds> [baseline rev, default origin/main]
#
# AB_VARIANTS: space-separated `name:patch+patch[:cxxflags]`, patches from scripts/ab/, applied in
# order with `patch -p2` to a copy. The default is the same-size rebuild of #363 at three triggers.
#   #363  p25:rebuild_same_size.patch:-DUDM_DRIFT_PCT=25
#   #364  pb:block96.patch+step1_pullback.patch     (control: b96:block96.patch)
#   #366  ex:block96.patch+exact_counter.patch
# AB_MODES (default "round missmix hitmix"): scripts/ab/move_home.cpp's modes. `round` is the churn
# alone, `missmix`/`hitmix` a churn round and four lookups (read across a sawtooth, if there is one),
# `miss`/`hit` lookups only, on the table the churn left. ns/op is per round or per lookup.
# Sizes 52363, 838860 and 3355443 (load 0.799 after reserve), ten turnovers before the clock, four
# turnovers timed (3M lookups for miss/hit; n/10 was 40 us at 52363 and noise), with 0 and 1 writing hits per
# round. Binaries rotate per round; medians at the end. AB_SMOKE=1: smallest size, one mode, a tenth
# of a turnover. About 6 minutes per round with four variants and three modes.
set -euo pipefail
[ $# -ge 1 ] || { sed -n '2,20p' "$0"; exit 1; }
rounds=$1
rev=${2:-origin/main}
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk}
cxx=${CXX_MH:-clang++}
mkdir -p "$build"
variant_specs=${AB_VARIANTS:-p10:rebuild_same_size.patch:-DUDM_DRIFT_PCT=10 p25:rebuild_same_size.patch:-DUDM_DRIFT_PCT=25 p50:rebuild_same_size.patch:-DUDM_DRIFT_PCT=50}

variants=(base)
mkdir -p "$build/base/ankerl"
git -C "$root" show "$rev:include/ankerl/unordered_dense.h" >"$build/base/ankerl/unordered_dense.h"
git -C "$root" show "$rev:include/ankerl/stl.h" >"$build/base/ankerl/stl.h"
"$cxx" -O3 -DNDEBUG -std=c++17 -w -I"$build/base" -I"$build/base/ankerl" "$root/scripts/ab/move_home.cpp" -o "$build/bin_base"
for spec in $variant_specs; do
    IFS=: read -r name patches flags <<<"$spec"
    rm -rf "${build:?}/$name"
    mkdir -p "$build/$name/ankerl"
    cp "$root/include/ankerl/unordered_dense.h" "$root/include/ankerl/stl.h" "$build/$name/ankerl/"
    IFS=+ read -ra plist <<<"$patches"
    for p in "${plist[@]}"; do
        patch -s -d "$build/$name" -p2 <"$root/scripts/ab/$p" ||
            { echo "$p no longer applies to the header; the measurement it belongs to is dated" >&2; exit 1; }
    done
    # shellcheck disable=SC2086 # flags is a list
    "$cxx" -O3 -DNDEBUG -std=c++17 -w ${flags:-} -I"$build/$name" -I"$build/$name/ankerl" \
        "$root/scripts/ab/move_home.cpp" -o "$build/bin_$name"
    variants+=("$name")
done

read -ra modes <<<"${AB_MODES:-round missmix hitmix}"
if [ -n "${AB_SMOKE:-}" ]; then
    sizes=(52363); modes=("${modes[0]}"); turn=1; tfrac=10
else
    sizes=(52363 838860 3355443); turn=10; tfrac=1
fi
out="$build/raw.txt"
: >"$out"
for ((r = 0; r < rounds; ++r)); do
    for n in "${sizes[@]}"; do
        for h in 0 1; do
            for mode in "${modes[@]}"; do
                reps=$((4 * n / tfrac))
                case $mode in miss | hit) reps=$((3000000 / tfrac)) ;; esac
                # rotate the starting binary with the round
                for ((i = 0; i < ${#variants[@]}; ++i)); do
                    v=${variants[$(((i + r) % ${#variants[@]}))]}
                    ns=$(taskset -c "${AB_CORE:-2}" "$build/bin_$v" "$mode" "$n" "$turn" "$h" "$reps" | awk '{for(i=1;i<=NF;i++) if($i=="ns/op") print $(i-1)}')
                    echo "$v $n $h $mode $ns" | tee -a "$out"
                done
            done
        done
    done
done
echo "== medians, ns per round or lookup (lower is better); ratio = base / variant, above 1.00 the variant is faster"
python3 - "$out" "${variants[@]}" <<'PY'
import sys, statistics, collections
d = collections.defaultdict(list)
for line in open(sys.argv[1]):
    v, n, h, mode, ns = line.split()
    d[(n, h, mode, v)].append(float(ns))
variants = sys.argv[2:]
keys = sorted({k[:3] for k in d}, key=lambda k: (int(k[0]), k[1], k[2]))
print(f"{'entries':>8} {'hits':>4} {'mode':>8} " + " ".join(f"{v:>14}" for v in variants))
for k in keys:
    base = statistics.median(d[k + ("base",)])
    cells = []
    for v in variants:
        med = statistics.median(d[k + (v,)])
        cells.append(f"{med:8.2f}" + ("" if v == "base" else f" {base / med:5.3f}"))
    print(f"{k[0]:>8} {k[1]:>4} {k[2]:>8} " + " ".join(f"{c:>14}" for c in cells))
PY
