#!/usr/bin/env bash
# Instructions and cycles per try_emplace hit and per fresh insert, main's header against the
# working tree's, both compilers, one mode per binary (#305). Counts are in-process around the
# loop; see try_emplace_hit.cpp. Pinned with AB_CORE (default 2).
#
#   AB_BUILD=/home/martinus/gra/x scripts/ab/try_emplace_hit.sh [-r REV] [rounds]
set -euo pipefail
rev=origin/main
if [[ "${1:-}" == "-r" ]]; then rev=$2; shift 2; fi
rounds=${1:-3}
root=$(git rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
core=${AB_CORE:-2}
mkdir -p "$build/base/ankerl" "$build/cand/ankerl"
for f in unordered_dense.h stl.h; do
    git -C "$root" show "$rev:include/ankerl/$f" >"$build/base/ankerl/$f"
    cp "$root/include/ankerl/$f" "$build/cand/ankerl/$f"
done
for cxx in clang++ g++; do
    for side in base cand; do
        for mode in HIT MISS; do
            for key in INT STRING_KEY; do
                "$cxx" -O3 -DNDEBUG -std=c++17 -I"$build/$side" -D$mode -D$key \
                    "$root/scripts/ab/try_emplace_hit.cpp" -o "$build/$cxx-$side-$mode-$key" &
            done
        done
    done
done
wait
echo "compiler side mode key instr/op cycles/op"
for ((i = 0; i < rounds; ++i)); do
    for cxx in clang++ g++; do
        for mode in HIT MISS; do
            for key in INT STRING_KEY; do
                for side in base cand; do
                    printf '%s %s ' "$cxx" "$side"
                    taskset -c "$core" "$build/$cxx-$side-$mode-$key"
                done
            done
        done
    done
done
