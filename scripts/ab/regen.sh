#!/bin/bash
# Regenerate everything in doc/: the CSVs, the SVGs and the interactive page.
#
#   scripts/ab/regen.sh              # measure everything, then draw    (~4 hours)
#   scripts/ab/regen.sh --redraw     # draw from the CSVs already there (~2 seconds)
#   scripts/ab/regen.sh --quick      # measure coarsely, for checking the pipeline (~10 minutes)
#
#   -r REV   the "robin hood (main)" baseline   (default origin/main)
#   -j REV   the "4.8.1 (January)" baseline     (default 3234af2, where main stood on 1 Jan 2026)
#   -c CXX   compiler                           (default clang++)
#   -o CORE  core to pin the measurements to    (default 2)
#
# --redraw is the one to use after changing plot.py or dashboard.py: nothing is measured, so nothing
# moves, and a chart change can be reviewed against unchanged numbers.
#
# A full run is hours of measurement and it must have the machine to itself -- everything else it
# does lands somewhere in the numbers. It is pinned to one core for that reason, which helps and does
# not make it safe to compile on the others meanwhile.
set -euo pipefail

# Bash reads a script as it runs it, so editing this file while it is running resumes execution at a
# shifted offset and produces nonsense -- which happened, three hours into a run. Re-exec from a copy
# once, so the copy is what bash is reading and the original is free to be edited.
if [ "${AB_REEXEC:-}" != "1" ]; then
    snapshot=$(mktemp /tmp/regen.XXXXXX.sh)
    cp "$0" "$snapshot"
    # The copy is outside the repository, so the root has to travel with it.
    AB_ROOT=$(git -C "$(dirname "$0")" rev-parse --show-toplevel) AB_REEXEC=1 exec bash "$snapshot" "$@"
fi
trap 'rm -f "$0"' EXIT # the snapshot is this file; bash keeps its fd, so removing it now is safe

redraw=0 quick=0 rev=origin/main jan=3234af2 cxx=clang++ core=2
while [ $# -gt 0 ]; do
    case $1 in
        --redraw) redraw=1 ;;
        --quick) quick=1 ;;
        -r) rev=$2; shift ;;
        -j) jan=$2; shift ;;
        -c) cxx=$2; shift ;;
        -o) core=$2; shift ;;
        *) sed -n '2,17p' "$0"; exit 1 ;;
    esac
    shift
done

root=${AB_ROOT:-$(git -C "$(dirname "$0")" rev-parse --show-toplevel)}
cd "$root"
doc=$root/doc
plot=$root/scripts/ab/plot.py

draw() { # csv out title subtitle [extra flags...]
    local csv=$1 out=$2 title=$3 sub=$4
    shift 4
    [ -f "$doc/$csv" ] || { echo "skip $out: no $csv" >&2; return 0; }
    "$plot" "$doc/$csv" "$doc/$out" "$title" "$sub" "$@" >/dev/null
    echo "  $out"
}

draw_all() {
    echo "drawing:"
    for k in "" _str; do
        case $k in "") kt="uint64_t keys" ;; *) kt="std::string keys" ;; esac
        draw "find_hits_vs_size$k.csv" "find_hits_vs_size$k.svg" \
            "Find, every lookup hitting" "nanoseconds per lookup, $kt"
        draw "find_vs_size$k.csv" "find_vs_size$k.svg" \
            "Find, half the lookups hitting (do not decide on this one)" "nanoseconds per lookup, $kt"
        draw "churn_vs_size$k.csv" "churn_vs_size$k.svg" \
            "Churn at a fixed size" "nanoseconds per erase-and-insert pair, $kt"
        draw "insert_erase_vs_size$k.csv" "insert_erase_vs_size$k.svg" \
            "Insert and erase" "nanoseconds per operator[] and erase pair, $kt"
        draw "memory_vs_value_size$k.csv" "memory_vs_value_size$k.svg" \
            "Memory against mapped-value size" "megabytes held, $kt" \
            "--panels=steady:steady state|peak:peak during growth" "--unit=MB" \
            "--x=sizeof(mapped_type), bytes" "--of=map&lt;K, T&gt;" --bars
        draw "memory_vs_size$k.csv" "memory_vs_size$k.svg" \
            "Memory against table size" "megabytes held, $kt" \
            "--panels=steady:steady state|peak:peak during growth" "--unit=MB" --logy
    done
    draw find_vs_size.csv find_ratio_vs_size.svg \
        "How much faster than robin hood" "times faster than the index this replaces, paired" ratio

    # The two value-size charts put a key type in each panel, so the long-form CSVs are joined into
    # one wide table first: build and iterate are separate charts because they differ by two orders
    # of magnitude and on one axis the iteration would be a flat line along the floor.
    if [ -f "$doc/value_size.csv" ]; then
        python3 - "$doc" <<'PY'
import csv, os, sys
doc = sys.argv[1]
def load(name):
    path = os.path.join(doc, name)
    if not os.path.exists(path):
        return {}
    out = {}
    for r in csv.DictReader(open(path)):
        out.setdefault((int(r["entries"]), r["map"]), {})[r["what"]] = r["ns"]
    return out
u64, st = load("value_size.csv"), load("value_size_str.csv")
with open(os.path.join(doc, ".value_size_wide.csv"), "w") as f:
    f.write("entries,map,build_u64,build_str,iterate_u64,iterate_str\n")
    for k in sorted(u64):
        a, b = u64[k], st.get(k, {})
        f.write(f"{k[0]},{k[1]},{a['build']},{b.get('build', a['build'])},"
                f"{a['iterate']},{b.get('iterate', a['iterate'])}\n")
PY
        draw .value_size_wide.csv build_vs_value_size.svg \
            "Build from empty, against mapped-value size" \
            "nanoseconds per entry, 200000 entries, nothing reserved" \
            "--panels=build_u64:uint64_t keys|build_str:std::string keys" \
            "--x=sizeof(mapped_type), bytes" "--of=map&lt;K, T&gt;" --bars
        draw .value_size_wide.csv iterate_vs_value_size.svg \
            "One iteration pass, against mapped-value size" \
            "nanoseconds per entry, 200000 entries" \
            "--panels=iterate_u64:uint64_t keys|iterate_str:std::string keys" \
            "--x=sizeof(mapped_type), bytes" "--of=map&lt;K, T&gt;" --bars
        rm -f "$doc/.value_size_wide.csv"
    fi
    "$root/scripts/ab/dashboard.py"
}

if [ $redraw = 1 ]; then
    draw_all
    exit 0
fi

build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"

# The sweep asks nanobench for a precision rather than a round count, which needs
# Bench::targetIntervalWidth and render(CompareResult) -- martinus/nanobench#189, not in the vendored
# 4.6.0. Say so here rather than let the compiler say it in three hundred lines.
nb=${NANOBENCH_INCLUDE:-$root/test}
if ! grep -q targetIntervalWidth "$nb/third-party/nanobench.h" 2>/dev/null; then
    cat >&2 <<MSG
error: the nanobench at $nb/third-party/nanobench.h has no targetIntervalWidth().

  The sweep chooses its round count by asking for an interval width, which needs
  martinus/nanobench#189. Until that lands, point at a checkout of it:

      NANOBENCH_INCLUDE=/path/to/nanobench/src scripts/ab/regen.sh

  (the directory holding third-party/nanobench.h, or src/include renamed to match)
MSG
    exit 1
fi

echo "baselines: main=$rev jan=$jan, compiler=$cxx, build dir $build"
sed_rename() { # revision namespace -> stdout
    git -C "$root" show "$1:include/ankerl/unordered_dense.h" \
        | sed "s/ankerl::unordered_dense/$2::unordered_dense/g;
               s/ANKERL_UNORDERED_DENSE/$(echo "$2" | tr '[:lower:]' '[:upper:]')_UNORDERED_DENSE/g;
               s/namespace ankerl/namespace $2/g;
               s|#        include \"stl.h\"|#        include <ankerl/stl.h>|"
}
sed_rename "$rev" udmbase > "$build/base.h"
sed_rename "$jan" udmjan > "$build/base_jan.h"

flags=(-O3 -DNDEBUG -std=c++17 -DUDM_AB_HAVE_JAN -I"$build" -I"$nb" -I"$root/include" -I"$root/test")
if echo '#include <boost/unordered/unordered_flat_map.hpp>
int main() {}' | "$cxx" -x c++ -std=c++17 -fsyntax-only - 2>/dev/null; then
    flags+=(-DUDM_AB_HAVE_BOOST)
else
    echo "note: boost not found, its line will be missing from every chart" >&2
fi

[ -f "$build/nanobench.o" ] || {
    printf '#define ANKERL_NANOBENCH_IMPLEMENT\n#include <third-party/nanobench.h>\n' > "$build/nb.cpp"
    "$cxx" "${flags[@]}" -c "$build/nb.cpp" -o "$build/nanobench.o"
}
for t in sweep valuesize memory; do
    "$cxx" "${flags[@]}" "$root/scripts/ab/$t.cpp" "$build/nanobench.o" -o "$build/$t"
done
echo "built sweep, valuesize, memory"

# Strings are sampled less finely and stop an octave earlier: a string operation costs about four
# times an integer one, and four sweeps at the integer settings would be most of a day. The shape is
# what these charts are for, and eight points per octave still shows the sawtooth.
if [ $quick = 1 ]; then
    shift_max=14 per_octave=3 width=0.08 entries=50000
    s_shift=13 s_octave=3 s_width=0.10 s_entries=20000
else
    shift_max=20 per_octave=12 width=0.03 entries=200000
    s_shift=19 s_octave=8 s_width=0.04 s_entries=200000
fi
run() { echo "  $1 ..." >&2; taskset -c "$core" "$build/${@:2}"; }

echo "measuring (pinned to core $core; leave the machine alone):"
run find_hits    sweep "$shift_max" "$per_octave" 20000 3 "$width" 0 > "$doc/find_hits_vs_size.csv"
run find         sweep "$shift_max" "$per_octave" 20000 0 "$width" 0 > "$doc/find_vs_size.csv"
run churn        sweep "$shift_max" "$per_octave" 20000 1 "$width" 0 > "$doc/churn_vs_size.csv"
run insert_erase sweep "$shift_max" "$per_octave" 20000 2 "$width" 0 > "$doc/insert_erase_vs_size.csv"
run memory_size  memory "$shift_max" 6 0 0 0                         > "$doc/memory_vs_size.csv"
run value_size   valuesize "$entries" "$width" 0                     > "$doc/value_size.csv"
run memory_value memory 0 0 1 1000000 0                              > "$doc/memory_vs_value_size.csv"

echo "the same again with std::string keys:"
run find_hits_str    sweep "$s_shift" "$s_octave" 20000 3 "$s_width" 1 > "$doc/find_hits_vs_size_str.csv"
run find_str         sweep "$s_shift" "$s_octave" 20000 0 "$s_width" 1 > "$doc/find_vs_size_str.csv"
run churn_str        sweep "$s_shift" "$s_octave" 20000 1 "$s_width" 1 > "$doc/churn_vs_size_str.csv"
run insert_erase_str sweep "$s_shift" "$s_octave" 20000 2 "$s_width" 1 > "$doc/insert_erase_vs_size_str.csv"
run memory_size_str  memory "$s_shift" 6 0 0 1                         > "$doc/memory_vs_size_str.csv"
run value_size_str   valuesize "$s_entries" "$s_width" 1               > "$doc/value_size_str.csv"
run memory_value_str memory 0 0 1 200000 1                             > "$doc/memory_vs_value_size_str.csv"

draw_all
