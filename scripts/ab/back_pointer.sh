#!/bin/bash
# The slot back-pointer (#266) across the size axis, per workload, string and integer keys.
#
#   scripts/ab/back_pointer.sh [-c COMPILER] [-r ROUNDS] [sizes...]
#
# Builds one binary holding both variants: the working tree's header, and a second copy of it
# compiled with ANKERL_UNORDERED_DENSE_SLOT_BACK_POINTER=1 and renamed into its own namespace, the
# way run.sh makes base.h. One process measures one cell, rounds are interleaved variant by variant
# so drift cannot land on one of them, and what is reported is the median of the rounds.
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
# The patch's switch defaults to off and the macro is renamed with everything else, so defining
# UDMBP_UNORDERED_DENSE_SLOT_BACK_POINTER turns it on for this copy and for nothing else.
cp "$root/include/ankerl/unordered_dense.h" "$build/bp_raw.h"
patch -s -p3 "$build/bp_raw.h" < "$root/scripts/ab/back_pointer.patch" ||
    { echo "back_pointer.patch no longer applies to the header -- the measurement it belongs to is" >&2
      echo "dated, see notes/index-design.md, \"A slot back-pointer, re-tested across the cache boundary\"" >&2
      exit 1; }
sed 's/ankerl::unordered_dense/udmbp::unordered_dense/g; s/ANKERL_UNORDERED_DENSE/UDMBP_UNORDERED_DENSE/g; s/namespace ankerl/namespace udmbp/g; s|#        include "stl.h"|#        include <ankerl/stl.h>|' \
    "$build/bp_raw.h" > "$build/bp.h"

flags=(-O3 -DNDEBUG -std=c++17 -DUDMBP_UNORDERED_DENSE_SLOT_BACK_POINTER=1 -I"$build" -I"$root/include" -I"$root/test")
[ -f "$build/nanobench_$cxx.o" ] || (cd "$build" && printf '#define ANKERL_NANOBENCH_IMPLEMENT\n#include <third-party/nanobench.h>\n' > nb.cpp && "$cxx" "${flags[@]}" -c nb.cpp -o "nanobench_$cxx.o")
# One variant per binary, for the reason window.cpp gives: two headers in one translation unit share
# an inlining budget, and a change that alters what is inlined reads the wrong sign there. Measured
# on this very change -- the integer erase-by-iterator cell reads 1.77 with both variants in one
# binary and 1.41 with one each, and the find control, which the change cannot reach, moves 2% in
# the first and up to 10% in the second. Neither is clean; they agree on what is large.
for side in 0 1; do
    "$cxx" "${flags[@]}" -DBP_ONE_SIDE=$side "$root/scripts/ab/back_pointer.cpp" "$build/nanobench_$cxx.o" \
        -o "$build/back_pointer_$side"
done
echo "back-pointer against the working tree's header, $cxx, $rounds rounds, one variant per binary, in $build" >&2

run() {
    local side=0
    [ "$1" = bp ] && side=1
    ${AB_CORE:+taskset -c $AB_CORE} "$build/back_pointer_$side" "$@"
}
median() { sort -g | awk '{v[NR]=$1} END {print (NR%2) ? v[(NR+1)/2] : (v[NR/2]+v[NR/2+1])/2}'; }

for keys in str u64; do
    echo
    echo "== $keys keys, ns per operation, and bp/base (below 1.00 means the back-pointer is faster)"
    printf "%-10s %10s %12s %12s %8s\n" "workload" "n" "base" "bp" "bp/base"
    for work in build churn erasekey eraseiter find; do
        for n in "${sizes[@]}"; do
            b=$(for _ in $(seq 1 "$rounds"); do run base "$keys" "$work" "$n"; done | median)
            c=$(for _ in $(seq 1 "$rounds"); do run bp "$keys" "$work" "$n"; done | median)
            printf "%-10s %10s %12.2f %12.2f %8.3f\n" "$work" "$n" "$b" "$c" \
                "$(awk -v b="$b" -v c="$c" 'BEGIN{print (b>0)? c/b : 0}')"
        done
    done
    echo "-- bytes per entry (one build, counted allocations)"
    printf "%-10s %10s %12s %12s %8s\n" "memory" "n" "base" "bp" "bp/base"
    for n in "${sizes[@]}"; do
        b=$(run base "$keys" mem "$n")
        c=$(run bp "$keys" mem "$n")
        printf "%-10s %10s %12.2f %12.2f %8.3f\n" "memory" "$n" "$b" "$c" \
            "$(awk -v b="$b" -v c="$c" 'BEGIN{print c/b}')"
    done
done
