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

root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
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
    draw find_hits_vs_size.csv find_hits_vs_size.svg \
        "Cost of a find that hits, against table size" "nanoseconds per lookup, every one of them present"
    draw find_vs_size.csv find_vs_size.svg \
        "Cost of a random find against table size" "nanoseconds per lookup, 50% of them hits"
    draw find_vs_size.csv find_ratio_vs_size.svg \
        "How much faster than robin hood" "times faster than the index this replaces, paired" ratio
    draw churn_vs_size.csv churn_vs_size.svg \
        "Cost of churn against table size" "nanoseconds per erase-and-insert pair at a fixed size"
    draw insert_erase_vs_size.csv insert_erase_vs_size.svg \
        "Cost of insert and erase against table size" \
        "nanoseconds per operator[] and erase pair, half of each finding nothing"
    # value_size.csv is long-form; the panel plotter wants one column per workload
    if [ -f "$doc/value_size.csv" ]; then
        python3 - "$doc/value_size.csv" "$doc/.value_size_wide.csv" <<'PY'
import csv, sys
rows = list(csv.DictReader(open(sys.argv[1])))
wide = {}
for r in rows:
    wide.setdefault((int(r["entries"]), r["map"]), {})[r["what"]] = r["ns"]
with open(sys.argv[2], "w") as f:
    f.write("entries,map,build,iterate\n")
    for (n, m), v in sorted(wide.items()):
        f.write(f"{n},{m},{v['build']},{v['iterate']}\n")
PY
        draw .value_size_wide.csv value_size.svg \
            "Build and iteration against mapped-value size" \
            "nanoseconds per entry, 200000 entries, nothing reserved" \
            "--panels=build:build from empty|iterate:one iteration pass" \
            "--x=sizeof(mapped_type), bytes" "--of=map&lt;uint64_t, T&gt;"
        rm -f "$doc/.value_size_wide.csv"
    fi
    draw memory_vs_value_size.csv memory_vs_value_size.svg \
        "Memory against mapped-value size" "megabytes held for 1000000 entries, nothing reserved" \
        "--panels=steady:steady state|peak:peak during growth" "--unit=MB" \
        "--x=sizeof(mapped_type), bytes" "--of=map&lt;uint64_t, T&gt;"
    draw memory_vs_size.csv memory_vs_size.svg \
        "Memory against table size" "megabytes held, nothing reserved" \
        "--panels=steady:steady state|peak:peak during growth" "--unit=MB" --logy
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

if [ $quick = 1 ]; then shift_max=14 per_octave=3 width=0.08 entries=50000
else shift_max=20 per_octave=12 width=0.02 entries=200000; fi
run() { echo "  $1 ..." >&2; taskset -c "$core" "$build/${@:2}"; }

echo "measuring (pinned to core $core; leave the machine alone):"
run find_hits    sweep "$shift_max" "$per_octave" 20000 3 "$width" > "$doc/find_hits_vs_size.csv"
run find         sweep "$shift_max" "$per_octave" 20000 0 "$width" > "$doc/find_vs_size.csv"
run churn        sweep "$shift_max" "$per_octave" 20000 1 "$width" > "$doc/churn_vs_size.csv"
run insert_erase sweep "$shift_max" "$per_octave" 20000 2 "$width" > "$doc/insert_erase_vs_size.csv"
run value_size   valuesize "$entries" "$width"                     > "$doc/value_size.csv"
run memory_value memory "$shift_max" 6 1 1000000                   > "$doc/memory_vs_value_size.csv"
run memory_size  memory "$shift_max" 6                             > "$doc/memory_vs_size.csv"

draw_all
