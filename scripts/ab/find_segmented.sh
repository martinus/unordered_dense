#!/usr/bin/env bash
# Instructions and cycles per find() and contains(), map against segmented_map, hits and misses,
# both key types, 50000 and 1M entries, both compilers, one cell per binary (#312). With -r, also
# builds REV's header and prints its row before the working tree's. Pinned with AB_CORE (default 2).
#
#   AB_BUILD=/home/martinus/gra/x scripts/ab/find_segmented.sh [-r REV] [rounds]
#
# Prints: compiler side map op mode key size instr/op cycles/op
set -euo pipefail
rev=
if [[ "${1:-}" == "-r" ]]; then rev=$2; shift 2; fi
rounds=${1:-1}
root=$(git rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
core=${AB_CORE:-2}
sides=(cand)
mkdir -p "$build/cand/ankerl"
cp "$root/include/ankerl/unordered_dense.h" "$root/include/ankerl/stl.h" "$build/cand/ankerl/"
if [[ -n "$rev" ]]; then
    sides=(base cand)
    mkdir -p "$build/base/ankerl"
    for f in unordered_dense.h stl.h; do
        git -C "$root" show "$rev:include/ankerl/$f" >"$build/base/ankerl/$f"
    done
fi
cells=()
for map in MAP SEGMENTED; do
    for op in FIND CONTAINS; do
        for mode in HIT MISS; do
            for key in INT STRING_KEY; do
                for size in 50000 1000000; do
                    cells+=("$map-$op-$mode-$key-$size")
                done
            done
        done
    done
done
for cxx in clang++ g++; do
    for side in "${sides[@]}"; do
        for c in "${cells[@]}"; do
            IFS=- read -r map op mode key size <<<"$c"
            "$cxx" -O3 -DNDEBUG -std=c++17 -I"$build/$side" -D$map -D$op -D$mode -D$key -DSIZE=$size \
                "$root/scripts/ab/find_segmented.cpp" -o "$build/$cxx-$side-$c" &
        done
        wait
    done
done
echo "compiler side map op mode key size instr/op cycles/op"
for ((i = 0; i < rounds; ++i)); do
    for cxx in clang++ g++; do
        for c in "${cells[@]}"; do
            for side in "${sides[@]}"; do
                printf '%s %s ' "$cxx" "$side"
                taskset -c "$core" "$build/$cxx-$side-$c"
            done
        done
    done
done
