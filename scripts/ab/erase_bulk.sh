#!/bin/bash
# erase_if and erase(first, last): the shipped loop of single erases against erase_bulk.patch, which
# counts what goes first and rebuilds the index once above a gate (#388).
#
#   AB_CORE=2 AB_BUILD=/home/martinus/gra/x scripts/ab/erase_bulk.sh [-c g++] [-r 5] [check|if|range ...]
#
# The patch is applied to a copy of the header, so the working tree's header must be unpatched. One
# binary per benchmark; inside it the variants are template parameters (the current loop, each mode
# forced, the gate) and their rounds alternate. `check` builds erase_bulk_check.cpp with
# ASan+UBSan and compares the prototypes with the current functions on random maps: same values in
# the same order, same returned iterator, every lookup agrees.
#
# Prints ns per element of the map before the erase and current/prototype, so above 1.00 the
# prototype is faster. -r 1 is a smoke of every cell (~2 min per benchmark); -r 5 takes ~15 min per
# benchmark and compiler. ANKERL_UD_ERASE_FACTOR and ANKERL_UD_ERASE_AHEAD in AB_DEFS move the
# gate and the lookahead: AB_DEFS="-DANKERL_UD_ERASE_FACTOR=4".
set -uo pipefail
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
what=("$@")
[ ${#what[@]} -gt 0 ] || what=(check if range)

root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
mkdir -p "$build/ankerl" || exit 1
cp "$root/include/ankerl/unordered_dense.h" "$root/include/ankerl/stl.h" "$build/ankerl/" || exit 1
patch -s -p3 "$build/ankerl/unordered_dense.h" < "$root/scripts/ab/erase_bulk.patch" ||
    { echo "erase_bulk.patch no longer applies to the header" >&2; exit 1; }

run() { ${AB_CORE:+taskset -c "$AB_CORE"} "$@"; }
for w in "${what[@]}"; do
    case $w in
        check)
            "$cxx" -O1 -g -fsanitize=address,undefined -D_GLIBCXX_ASSERTIONS -std=c++20 ${AB_DEFS:-} -I"$build" \
                "$root/scripts/ab/erase_bulk_check.cpp" -o "$build/erase_bulk_check" || exit 1
            stdbuf -o0 "$build/erase_bulk_check" || { echo "check failed" >&2; exit 1; }
            ;;
        if | range)
            "$cxx" -O3 -DNDEBUG -std=c++20 ${AB_DEFS:-} -I"$build" "$root/scripts/ab/erase_$w.cpp" -o "$build/erase_$w" || exit 1
            echo "== erase_$w, $cxx, $rounds rounds"
            run "$build/erase_$w" "$rounds" | tee "$build/erase_${w}_$(basename "$cxx").txt"
            ;;
        *) echo "unknown: $w" >&2; exit 1 ;;
    esac
done
