#!/bin/bash
# The slot back-pointer (#266) across the size axis, per workload, string, integer and big-value keys.
#
#   scripts/ab/back_pointer.sh [-c COMPILER] [-r ROUNDS] [sizes...]
#
# The shipped header against itself with back_pointer.patch applied. One variant per binary, because
# two headers in one translation unit share an inlining budget and this change alters finish_erase's
# size -- see scripts/ab/window.cpp. Rounds alternate the two variants, so drift between rounds
# cannot land on whichever one runs second, and each cell reports the median with its spread.
#
# AB_BUILD picks the build directory (default: a temporary one); AB_CORE pins the measured process.
set -euo pipefail
export LC_ALL=C
cxx=clang++ rounds=5
while getopts "c:r:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        r) rounds=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
sizes=("$@")
[ ${#sizes[@]} -gt 0 ] || sizes=(50000 200000 800000 2000000 4000000)

root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"

# The second variant: the shipped header with back_pointer.patch applied, in a namespace of its own.
# The patch's switch defaults to off and the macro is renamed with everything else (the same sed
# run.sh uses to make base.h), so defining UDMBP_UNORDERED_DENSE_SLOT_BACK_POINTER turns it on for
# this copy and for nothing else.
cp "$root/include/ankerl/unordered_dense.h" "$build/bp_raw.h"
patch -s -p3 "$build/bp_raw.h" < "$root/scripts/ab/back_pointer.patch" ||
    { echo "back_pointer.patch no longer applies to the header -- the measurement it belongs to is" >&2
      echo "dated; the commit it was taken against is named at the top of the patch" >&2
      exit 1; }
sed 's/ankerl::unordered_dense/udmbp::unordered_dense/g; s/ANKERL_UNORDERED_DENSE/UDMBP_UNORDERED_DENSE/g; s/namespace ankerl/namespace udmbp/g; s|#        include "stl.h"|#        include <ankerl/stl.h>|' \
    "$build/bp_raw.h" > "$build/bp.h"

flags=(-O3 -DNDEBUG -std=c++17 -DUDMBP_UNORDERED_DENSE_SLOT_BACK_POINTER=1 -I"$build" -I"$root/include" -I"$root/test")
for side in 0 1; do
    "$cxx" "${flags[@]}" -DBP_ONE_SIDE=$side "$root/scripts/ab/back_pointer.cpp" "$root/test/app/nanobench.cpp" \
        -o "$build/back_pointer_$side"
done
echo "back-pointer against the working tree's header, $cxx, $rounds rounds, one variant per binary, in $build" >&2

run() { ${AB_CORE:+taskset -c $AB_CORE} "$build/back_pointer_$1" "${@:2}"; }
stats() { sort -g | awk '{v[NR]=$1} END {printf "%.2f %.2f %.2f", (NR%2)?v[(NR+1)/2]:(v[NR/2]+v[NR/2+1])/2, v[1], v[NR]}'; }

# base and bp alternate inside one round, so a machine that drifts across the cell drifts across
# both of them rather than across the second one only.
cell() {
    local keys=$1 work=$2 n=$3 r
    : > "$build/b.txt"
    : > "$build/c.txt"
    for r in $(seq 1 "$rounds"); do
        run 0 base "$keys" "$work" "$n" >> "$build/b.txt"
        run 1 bp "$keys" "$work" "$n" >> "$build/c.txt"
    done
    local b c
    b=$(stats < "$build/b.txt")
    c=$(stats < "$build/c.txt")
    awk -v w="$work" -v n="$n" -v b="$b" -v c="$c" 'BEGIN {
        split(b, bb, " "); split(c, cc, " ")
        printf "%-10s %10s %10.2f %10.2f %8.3f   [%.2f-%.2f vs %.2f-%.2f]\n", w, n, bb[1], cc[1], cc[1]/bb[1], bb[2], bb[3], cc[2], cc[3]
    }'
}

for keys in str u64 big; do
    echo
    echo "== $keys keys, ns per operation, bp/base (below 1.00 means the back-pointer is faster), min-max of the rounds"
    printf "%-10s %10s %10s %10s %8s\n" "workload" "n" "base" "bp" "bp/base"
    for work in build churn erasekey eraseiter find; do
        for n in "${sizes[@]}"; do cell "$keys" "$work" "$n"; done
    done
    echo "-- live bytes per entry, allocations minus frees, one build (deterministic, one round)"
    for n in "${sizes[@]}"; do
        b=$(run 0 base "$keys" mem "$n")
        c=$(run 1 bp "$keys" mem "$n")
        printf "%-10s %10s %10.2f %10.2f %8.3f\n" "memory" "$n" "$b" "$c" "$(awk -v b="$b" -v c="$c" 'BEGIN{print c/b}')"
    done
done
