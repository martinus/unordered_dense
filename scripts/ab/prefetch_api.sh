#!/bin/bash
# What prefetch(key) is worth, against depth and table size.
#
#   scripts/ab/prefetch_api.sh [-c COMPILER] [-k u64|str] [-n "200000 4000000"] [-d "0 4 8 16"] [-r REPS]
#
# depth 0 is the same loop with no prefetching, so the two sides differ only in the prefetch.
# Figures are the slope of two repetition counts: building the table is not free and perf counts
# the whole process.
set -euo pipefail
export LC_ALL=C
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"
cxx=clang++ keys=u64 sizes="200000 1000000 4000000 16000000" depths="0 4 8 16 32" reps=8000000
while getopts "c:k:n:d:r:" opt; do
    case $opt in
        c) cxx=$OPTARG ;; k) keys=$OPTARG ;; n) sizes=$OPTARG ;; d) depths=$OPTARG ;; r) reps=$OPTARG ;;
        *) exit 1 ;;
    esac
done
kf=""; [ "$keys" = str ] && kf="-DUDM_PF_STR"
# shellcheck disable=SC2086 -- PF_EXTRA_FLAGS is meant to split
"$cxx" -O3 -DNDEBUG -std=c++17 -w ${PF_EXTRA_FLAGS:-} -I"$root/include" -I"$root/test" ${kf:+"$kf"} \
    "$root/scripts/ab/prefetch_api.cpp" -o "$build/prefetch_api"

# The binary times its own loop, so nothing here needs perf -- which is what lets this run on
# macOS, and macOS is where the answer is most likely to differ.
run() { # work depth n mode -> ns per lookup
    ${AB_CORE:+taskset -c "$AB_CORE"} "$build/prefetch_api" "$1" "$2" "$3" "$reps" "$4" |
        awk '{ printf "%8.2f", $1 }'
}

# `hash` pipelines the key fetch and the hash and touches no map memory; `prefetch` also fetches
# the block. The gap between the two columns at one depth is what prefetch() itself is worth.
echo "keys=$keys  $cxx  reps=$reps   ns per lookup   (h = hash_for ahead, p = prefetch ahead)"
printf "%-10s %-6s %8s" n work "d=0"
for d in $depths; do [ "$d" = 0 ] && continue; printf " %8s %8s" "${d}h" "${d}p"; done
echo
for n in $sizes; do
    for work in hit half; do
        base=$(run "$work" 0 "$n" prefetch)
        printf "%-10s %-6s %8s" "$n" "$work" "$base"
        for d in $depths; do
            [ "$d" = 0 ] && continue
            printf " %8s %8s" "$(run "$work" "$d" "$n" hash)" "$(run "$work" "$d" "$n" prefetch)"
        done
        echo
    done
done
