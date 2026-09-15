#!/bin/bash
# The rehash loop's pipeline across the size axis, with the small sizes it has never been measured
# at.
#
#   scripts/ab/rehash_size.sh [-c COMPILER] [-r ROUNDS] [-k u64|str] [-m rehash|build] [sizes...]
#
# -m rehash times rehash(0) on a settled map, which isolates the loop. -m build times a build from
# empty, which is where the loop actually runs and says whether the isolated number reaches a caller.
#
# The shipped header against a copy with scripts/ab/rehash_plain.patch applied, which is the same
# loop with the ring and the prefetch taken out. One variant per binary, because a ring changes the
# function's size and two headers in one translation unit share an inlining budget. Rounds alternate
# the two binaries, so drift between rounds cannot land on whichever one runs second, and each cell
# reports the median of its rounds.
#
# Prints ns per element for both variants and plain/pipelined, so a ratio above 1 means the pipeline
# is winning and below 1 means it is costing.
#
# AB_BUILD picks the build directory (default: a temporary one); AB_CORE pins the measured process.
set -euo pipefail
export LC_ALL=C
cxx=clang++ rounds=5 key=u64 mode=rehash
while getopts "c:r:k:m:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        r) rounds=$OPTARG ;;
        k) key=$OPTARG ;;
        m) mode=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
sizes=("$@")
# Four points per octave from 1000 to 512000: a rehash places every element, so its cost per element
# follows the load factor, which sweeps from about a half to the maximum between two doublings. One
# point per octave samples one phase of that sawtooth and draws a straight line through it.
[ ${#sizes[@]} -gt 0 ] || sizes=(1000 1414 2000 2828 4000 5657 8000 11314 16000 22627 32000 45255
                                 64000 90510 128000 181019 256000 362039 512000)

root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build/plain/ankerl"

# The control: the shipped header with the pipeline removed. stl.h comes along unchanged, because
# the copy still includes it by the relative name the shipped header uses.
cp "$root/include/ankerl/stl.h" "$build/plain/ankerl/stl.h"
cp "$root/include/ankerl/unordered_dense.h" "$build/plain/ankerl/unordered_dense.h"
patch -s -p3 "$build/plain/ankerl/unordered_dense.h" < "$root/scripts/ab/rehash_plain.patch" ||
    { echo "rehash_plain.patch no longer applies -- the loop has changed and the numbers this" >&2
      echo "script produced are dated. Re-derive the patch rather than force it." >&2
      exit 1; }

# One mode per binary: the two timed functions in one translation unit changed each other's
# codegen, see the comment at the top of rehash_size.cpp.
flags=(-O3 -DNDEBUG -std=c++17 -I"$root/test")
[ "$mode" = build ] && flags+=(-DREHASH_MODE_BUILD)
"$cxx" "${flags[@]}" -I"$root/include" -o "$build/rehash_pipelined" "$root/scripts/ab/rehash_size.cpp"
"$cxx" "${flags[@]}" -I"$build/plain" -o "$build/rehash_plain" "$root/scripts/ab/rehash_size.cpp"

declare -A runs
for _ in $(seq 1 "$rounds"); do
    for v in pipelined plain; do
        while IFS=, read -r n ns; do
            runs[$v,$n]+="$ns "
        done < <(${AB_CORE:+taskset -c $AB_CORE} "$build/rehash_$v" "$key" "$mode" "${sizes[@]}")
    done
done

median() { tr ' ' '\n' <<<"$1" | grep -v '^$' | sort -n |
    awk '{a[NR] = $1} END {printf "%.4f", (NR % 2 ? a[(NR + 1) / 2] : (a[NR / 2] + a[NR / 2 + 1]) / 2)}'; }

# How far the rounds of one cell spread, as a percentage of its median. A cell whose spread is
# comparable to the difference between the two variants has not measured that difference.
spread() { tr ' ' '\n' <<<"$1" | grep -v '^$' | sort -n |
    awk '{a[NR] = $1} END {m = (NR % 2 ? a[(NR + 1) / 2] : (a[NR / 2] + a[NR / 2 + 1]) / 2)
          printf "%.1f", (m > 0 ? 100 * (a[NR] - a[1]) / m : 0)}'; }

echo "$cxx, $key keys, $mode, $rounds rounds, ns per element"
printf '%10s  %10s %7s  %10s %7s  %8s\n' entries pipelined spread% plain spread% "plain/pl"
for n in "${sizes[@]}"; do
    p=$(median "${runs[pipelined,$n]}")
    q=$(median "${runs[plain,$n]}")
    printf '%10s  %10s %7s  %10s %7s  %8.4f\n' "$n" "$p" "$(spread "${runs[pipelined,$n]}")" \
        "$q" "$(spread "${runs[plain,$n]}")" "$(awk -v a="$q" -v b="$p" 'BEGIN {print a / b}')"
done
