#!/bin/bash
# The huge page allocator against std::allocator, interleaved round by round, medians reported.
#
#   scripts/ab/huge_pages.sh [-c COMPILER] [-k u64|str|big] [-w build|churn|find|ie] [-n SIZE]
#                            [-p] [rounds] [alloc...]
#
# Allocators are the modes huge_pages.cpp lists: std huge huge4 seg seghuge seghuge16 boost boosthuge.
# Defaults: clang++, u64 keys, churn, 800000 entries, five rounds, `std` against `huge`. -p runs
# every cell once more under `perf stat` and prints the two TLB counters that are the mechanism,
# per operation: L1 DTLB misses served by the L2 TLB on 4 KB pages, and page walks. AB_CORE pins.
#
# Read the size axis, not one cell: the allocator only puts blocks of 2 MB and up on huge pages,
# so a 50000-entry table gains nothing here and 5-8% under glibc's heap-wide madvise -- that is a
# statement about where the blocks are, not noise. See include/ankerl/huge_page_allocator.h.
set -euo pipefail
export LC_ALL=C
cxx=clang++ keys=u64 work=churn size=800000 perf=0
while getopts "c:k:w:n:p" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        k) keys=$OPTARG ;;
        w) work=$OPTARG ;;
        n) size=$OPTARG ;;
        p) perf=1 ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
rounds=${1:-5}
[ $# -gt 0 ] && shift
allocs=("$@")
[ ${#allocs[@]} -gt 0 ] || allocs=(std huge)

root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"
bin="$build/huge_pages_$(basename "$cxx")"
[ -x "$bin" ] || "$cxx" -O3 -DNDEBUG -std=c++17 -I"$root/include" -I"$root/test" -o "$bin" \
    "$root/scripts/ab/huge_pages.cpp" "$root/test/app/nanobench.cpp"

pin=${AB_CORE:+taskset -c $AB_CORE}
declare -A runs
for _ in $(seq 1 "$rounds"); do
    for a in "${allocs[@]}"; do
        runs[$a]+="$($pin "$bin" "$a" "$keys" "$work" "$size" | awk '{print $1}') "
    done
done

echo "$cxx, $keys keys, $work, $size entries, $rounds rounds, ns per operation"
for a in "${allocs[@]}"; do
    printf '  %-6s ' "$a"
    tr ' ' '\n' <<<"${runs[$a]}" | grep -v '^$' | sort -n |
        awk '{v[NR] = $1} END {printf "median %8.3f   min %8.3f   max %8.3f\n",
              (NR % 2 ? v[(NR + 1) / 2] : (v[NR / 2] + v[NR / 2 + 1]) / 2), v[1], v[NR]}'
done
if [ "$perf" = 1 ]; then
    for a in "${allocs[@]}"; do
        out=$($pin perf stat -x, -e ls_l1_d_tlb_miss.tlb_reload_4k_l2_hit,ls_l1_d_tlb_miss.all_l2_miss \
            -- "$bin" "$a" "$keys" "$work" "$size" 2>&1)
        ops=$(grep -o '[0-9]* ops' <<<"$out" | awk '{print $1}')
        printf '  %-6s per op: ' "$a"
        awk -F, -v ops="$ops" '/tlb_reload_4k_l2_hit/ {printf "L2 TLB hits %.3f   ", $1 / ops}
                               /all_l2_miss/ {printf "page walks %.4f\n", $1 / ops}' <<<"$out"
    done
fi
