#!/bin/bash
# What the order a segmented_map tears itself down in costs the build after it (see
# teardown_order.cpp). Builds REV's header (default origin/main), the working tree's, and the working
# tree's with scripts/ab/teardown_blocks_forward.patch applied (elements last first, blocks first
# first), for three key kinds, one binary each, then runs them alternated, PASSES times, pinned to
# AB_CORE (default 2).
#
#   AB_BUILD=/home/martinus/gra/x scripts/ab/teardown_order.sh [-r REV] [-c COMPILER] [PASSES] [ROUNDS] [SIZES...]
#
# Defaults: 3 passes, 7 rounds, sizes 50000 200000 1000000. About three minutes with clang.
set -euo pipefail
rev=origin/main
cxx=clang++
while getopts "r:c:" opt; do
    case $opt in
        r) rev=$OPTARG ;;
        c) cxx=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
passes=${1:-3}
rounds=${2:-7}
shift 2 || true
sizes=("$@")
[ ${#sizes[@]} -gt 0 ] || sizes=(50000 200000 1000000)
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
mkdir -p "$build/base/ankerl" "$build/cand/ankerl" "$build/blocksfwd/ankerl"
for f in unordered_dense.h stl.h; do
    git -C "$root" show "$rev:include/ankerl/$f" >"$build/base/ankerl/$f"
    cp "$root/include/ankerl/$f" "$build/cand/ankerl/$f"
    cp "$root/include/ankerl/$f" "$build/blocksfwd/ankerl/$f"
done
patch -s -d "$build/blocksfwd" -p2 <"$root/scripts/ab/teardown_blocks_forward.patch"
sides=(base cand blocksfwd)
kinds=(KIND_U64 KIND_STR KIND_OWNED)
for side in "${sides[@]}"; do
    for kind in "${kinds[@]}"; do
        "$cxx" -O3 -DNDEBUG -std=c++17 -I"$build/$side" -D$kind "$root/scripts/ab/teardown_order.cpp" -o "$build/$cxx-$side-$kind" &
    done
done
wait
echo "# $cxx, baseline $rev, $passes passes x $rounds rounds, sizes ${sizes[*]}"
for ((p = 0; p < passes; ++p)); do
    for kind in "${kinds[@]}"; do
        for side in "${sides[@]}"; do
            taskset -c "${AB_CORE:-2}" "$build/$cxx-$side-$kind" "$rounds" "${sizes[@]}" | sed "s/^/$side /"
        done
    done
done
