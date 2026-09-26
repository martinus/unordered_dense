#!/bin/bash
# What the order a segmented_map tears itself down in costs the build after it
# (scripts/ab/teardown_order.cpp). Three sides: REV's header (default origin/main), the working
# tree's, and the working tree's with scripts/ab/teardown_blocks_forward.patch applied (elements last
# first, blocks first first). Three key kinds each, one binary per side and kind, one process per
# size so that every first build is on a fresh heap, PASSES passes alternating the sides, pinned to
# AB_CORE (default 2). About three minutes with the defaults.
#
#   AB_BUILD=/home/martinus/gra/x scripts/ab/teardown_order.sh [-r REV] [-c COMPILER] [PASSES [ROUNDS [SIZES...]]]
#
# Defaults: 3 passes, 7 rounds, sizes 50000 200000 1000000.
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
shift $(($# < 2 ? $# : 2))
sizes=("$@")
[ ${#sizes[@]} -gt 0 ] || sizes=(50000 200000 1000000)
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
sides=(base cand blocksfwd)
kinds=(KIND_U64 KIND_STR KIND_OWNED)
for side in "${sides[@]}"; do
    mkdir -p "$build/$side/ankerl"
done
for f in unordered_dense.h stl.h; do
    git -C "$root" show "$rev:include/ankerl/$f" >"$build/base/ankerl/$f"
    cp "$root/include/ankerl/$f" "$build/cand/ankerl/$f"
    cp "$root/include/ankerl/$f" "$build/blocksfwd/ankerl/$f"
done
patch -s -d "$build/blocksfwd" -p2 <"$root/scripts/ab/teardown_blocks_forward.patch" ||
    { echo "teardown_blocks_forward.patch no longer applies: dealloc() has changed. Re-derive the" >&2
      echo "patch rather than force it." >&2
      exit 1; }
pids=()
for side in "${sides[@]}"; do
    for kind in "${kinds[@]}"; do
        rm -f "$build/$cxx-$side-$kind"
        "$cxx" -O3 -DNDEBUG -std=c++17 -I"$build/$side" -I"$root/test" -D$kind \
            "$root/scripts/ab/teardown_order.cpp" -o "$build/$cxx-$side-$kind" &
        pids+=($!)
    done
done
for pid in "${pids[@]}"; do
    wait "$pid"
done
echo "# $cxx, baseline $rev, $passes passes x $rounds rounds, sizes ${sizes[*]}"
for ((p = 0; p < passes; ++p)); do
    for kind in "${kinds[@]}"; do
        for n in "${sizes[@]}"; do
            for side in "${sides[@]}"; do
                taskset -c "${AB_CORE:-2}" "$build/$cxx-$side-$kind" "$rounds" "$n" | sed "s/^/$side /"
            done
        done
    done
done
