#!/bin/bash
# What clear() costs on a table that holds few entries against its groups, and where taking each
# entry out beats resetting every group (#381; scripts/ab/clear_sparse.cpp). Three sides: REV's
# header (default origin/main: every clear resets), the working tree's, and the working tree's with
# clear_per_entry_groups set to 1 (every clear of fewer entries than groups goes per entry). Four key
# kinds, one binary per side and kind, one process per cell, PASSES passes rotating the order of the
# sides, pinned to AB_CORE (default 2). Each cell is a peak (what the table was reserve()d to) and a
# ratio of groups to entries; the defaults step the ratio across the threshold at a peak inside
# every cache (10000: 1024 groups) and one far past it (4000000: 524288 groups), hot, cold and as a
# fill-and-clear loop, plus #381's first table (16 entries, peaks 10k to 4M, loop). About 40 minutes
# a pass per compiler on a Threadripper 3995WX.
#
#   AB_BUILD=/root/gra/x scripts/ab/clear_sparse.sh [-r REV] [-c COMPILER] [PASSES]
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
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
sides=(base cand perentry)
kinds=(KIND_U64 KIND_S16 KIND_S135 KIND_S1K)
for side in "${sides[@]}"; do
    mkdir -p "$build/$side/ankerl"
done
for f in unordered_dense.h stl.h; do
    git -C "$root" show "$rev:include/ankerl/$f" >"$build/base/ankerl/$f"
    cp "$root/include/ankerl/$f" "$build/cand/ankerl/$f"
    cp "$root/include/ankerl/$f" "$build/perentry/ankerl/$f"
done
sed -i 's/clear_per_entry_groups = [0-9]*;/clear_per_entry_groups = 1;/' "$build/perentry/ankerl/unordered_dense.h"
grep -q 'clear_per_entry_groups = 1;' "$build/perentry/ankerl/unordered_dense.h" ||
    { echo "clear_per_entry_groups not found in the working tree's header" >&2; exit 1; }
pids=()
for side in "${sides[@]}"; do
    for kind in "${kinds[@]}"; do
        rm -f "$build/$cxx-$side-$kind"
        "$cxx" -O3 -DNDEBUG -std=c++17 -I"$build/$side" -D$kind \
            "$root/scripts/ab/clear_sparse.cpp" -o "$build/$cxx-$side-$kind" &
        pids+=($!)
    done
done
for pid in "${pids[@]}"; do
    wait "$pid"
done
# peak:ratio:mode:samples
cells=()
for peak in 10000 4000000; do
    for ratio in 2 4 8 16 24 32 48 64 128 1024; do
        cells+=("$peak:$ratio:hot:201" "$peak:$ratio:cold:21" "$peak:$ratio:loop:7")
    done
done
cells+=("100000:512:loop:7" "1000000:8192:loop:7") # with 10000:64 and 4000000:32768 below: #381's first table
cells+=("4000000:32768:loop:7")
echo "# $cxx, baseline $rev, $passes passes, core ${AB_CORE:-2}, thp $(cat /sys/kernel/mm/transparent_hugepage/enabled)"
for ((p = 0; p < passes; ++p)); do
    for kind in "${kinds[@]}"; do
        for cell in "${cells[@]}"; do
            IFS=: read -r peak ratio mode samples <<<"$cell"
            for ((s = 0; s < ${#sides[@]}; ++s)); do
                side=${sides[$(((s + p) % ${#sides[@]}))]}
                taskset -c "${AB_CORE:-2}" "$build/$cxx-$side-$kind" "$peak" "$ratio" "$mode" "$samples" |
                    sed "s/^/pass=$p $side /"
            done
        done
    done
done
