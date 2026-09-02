#!/bin/bash
# Paired A/B of the working-tree header against a git revision of it (default: HEAD).
#
#   scripts/ab/run.sh [-r REV] [-b] [-c COMPILER] <workload|all> [epochs] 
#
#   -r REV       baseline revision (default HEAD)
#   -b           also measure boost::unordered_flat_map (needs boost headers)
#   -c COMPILER  default clang++
#
# Needs nanobench >= 4.6 for Bench::compare(); point NANOBENCH_INCLUDE at its include directory.
# Everything is built in $AB_BUILD (default: a temporary directory), the tree is not touched.
set -euo pipefail
rev=HEAD boost=0 cxx=clang++
while getopts "r:bc:" opt; do
    case $opt in
        r) rev=$OPTARG ;;
        b) boost=1 ;;
        c) cxx=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
[ $# -ge 1 ] || { sed -n '2,12p' "$0"; exit 1; }
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
nb=${NANOBENCH_INCLUDE:?set NANOBENCH_INCLUDE to a nanobench >= 4.6 include directory}
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"
# the baseline header, in its own namespace and macro prefix, beside the candidate one
git -C "$root" show "$rev:include/ankerl/unordered_dense.h" \
    | sed 's/ankerl::unordered_dense/udmbase::unordered_dense/g; s/ANKERL_UNORDERED_DENSE/UDMBASE_UNORDERED_DENSE/g; s/namespace ankerl/namespace udmbase/g; s|#        include "stl.h"|#        include <ankerl/stl.h>|' \
    > "$build/base.h"
flags=(-O3 -DNDEBUG -std=c++17 -I"$build" -I"$root/include" -I"$nb")
[ $boost = 1 ] && flags+=(-DUDM_AB_HAVE_BOOST)
[ -f "$build/nanobench_$cxx.o" ] || (cd "$build" && printf '#define ANKERL_NANOBENCH_IMPLEMENT\n#include <nanobench.h>\n' > nb.cpp && "$cxx" "${flags[@]}" -c nb.cpp -o "nanobench_$cxx.o")
"$cxx" "${flags[@]}" "$root/scripts/ab/ab.cpp" "$build/nanobench_$cxx.o" -o "$build/ab"
echo "baseline $rev vs working tree, $cxx, in $build" >&2
"$build/ab" "$1" "${2:-12}" "$boost"
