#!/bin/bash
# Paired A/B of the working-tree header against a git revision of it (default: HEAD).
#
#   scripts/ab/run.sh [-r REV] [-b] [-c COMPILER] [-p POINTS] <workload|all> [epochs] 
#
#   -r REV       baseline revision (default HEAD)
#   -b           also measure boost::unordered_flat_map (needs boost headers)
#   -c COMPILER  default clang++
#   -p POINTS    sizes per octave, default 5. Each size-sensitive workload is measured at POINTS
#                sizes from n to just under 2n and the geometric mean of those ratios is what is
#                reported, because a table's load factor sweeps a sawtooth between doublings and
#                two indexes double at different sizes -- so one fixed size measures wherever that
#                size happens to land on each. `-p 1` is the single size the score used before
#                2026-09-07; it runs five times faster and is only comparable with itself. `-p 50`
#                draws the sawtooth instead of averaging it out: 5m34 against the default's 2m21
#                here, where a tenfold count would have been 20 minutes. The two workloads that
#                grow a map from empty keep five points, having no sawtooth left to sample, and
#                the two lookup workloads search a table built once outside the timed region. The
#                sizes are template arguments, so -p rebuilds the binary: 17s of clang at fifty
#                points against 4s at five.
#
# Uses the vendored nanobench (test/third-party, >= 4.6 for Bench::compare()); NANOBENCH_INCLUDE
# overrides it. The workloads come from test/bench/workloads.h, the benchmark's own. Everything is built in $AB_BUILD (default: a temporary directory), the tree is not
# touched.
#
# AB_EXTRA_FLAGS is appended to the compile line of both sides, which is how a run turns a
# compile-time switch off for baseline and candidate together, e.g.
#
#   AB_EXTRA_FLAGS='-DANKERL_UNORDERED_DENSE_HAS_SSE2=0 -DUDMBASE_UNORDERED_DENSE_HAS_SSE2=0'
#
# Both prefixes are needed: the baseline header is rewritten into its own macro namespace, so
# defining only one of them would compare a scalar candidate against a vector baseline.
set -euo pipefail
rev=HEAD boost=0 cxx=clang++ points=5
while getopts "r:bc:p:" opt; do
    case $opt in
        r) rev=$OPTARG ;;
        b) boost=1 ;;
        c) cxx=$OPTARG ;;
        p) points=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
[ $# -ge 1 ] || { sed -n '2,18p' "$0"; exit 1; }
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"
# Windows will not run a PE without the extension, and bash under Git for Windows is where this
# gets invoked there. Everywhere else the suffix is empty and nothing changes.
exe=ab
case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN*) exe=ab.exe ;;
esac
# the baseline header, in its own namespace and macro prefix, beside the candidate one
git -C "$root" show "$rev:include/ankerl/unordered_dense.h" \
    | sed 's/ankerl::unordered_dense/udmbase::unordered_dense/g; s/ANKERL_UNORDERED_DENSE/UDMBASE_UNORDERED_DENSE/g; s/namespace ankerl/namespace udmbase/g; s|#        include "stl.h"|#        include <ankerl/stl.h>|' \
    > "$build/base.h"
flags=(-O3 -DNDEBUG -std=c++17 "-DUDM_AB_POINTS=$points" -I"$build" -I"$root/include" -I"$root/test")
# shellcheck disable=SC2206 -- word splitting is what makes AB_EXTRA_FLAGS able to carry several
flags+=(${AB_EXTRA_FLAGS:-})
[ $boost = 1 ] && flags+=(-DUDM_AB_HAVE_BOOST)
[ -f "$build/nanobench_$cxx.o" ] || (cd "$build" && printf '#define ANKERL_NANOBENCH_IMPLEMENT\n#include <third-party/nanobench.h>\n' > nb.cpp && "$cxx" "${flags[@]}" -c nb.cpp -o "nanobench_$cxx.o")
"$cxx" "${flags[@]}" "$root/scripts/ab/ab.cpp" "$build/nanobench_$cxx.o" -o "$build/$exe"
echo "baseline $rev vs working tree, $cxx, in $build" >&2
"$build/$exe" "$1" "${2:-12}" "$boost"
