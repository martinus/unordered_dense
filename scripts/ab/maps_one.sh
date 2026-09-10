#!/bin/bash
# Hardware counters, one map per binary: build every map's binary for one workload and run it under
# `perf stat`. The paired harness next door measures ratios; this measures mechanism.
#
#   scripts/ab/maps_one.sh [-c COMPILER] [-k u64|str|big] <workload> <entries> <reps> [map...]
#
# With no map named it does all of them. Netting out the loop is what the `none` workload is for.
# `reps` is a count of operations and its default in the binary is 30000000; a small value measures
# noise rather than the map.
#
# AB_CORE pins the measured binary to one core. Unset it and nothing is pinned, which is what this
# always did -- instruction counts do not need it and cycles do.
set -euo pipefail
export LC_ALL=C # a German locale prints 1,23 and every awk over perf output then reads 1
cxx=clang++ keys=u64
while getopts "c:k:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        k) keys=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
[ $# -ge 3 ] || { sed -n '2,8p' "$0"; exit 1; }
work=$1 n=$2 reps=$3
shift 3
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"
[ -f "$build/base411.h" ] || { echo "run scripts/ab/maps.sh first: $build/base411.h is what carries 4.11.0" >&2; exit 1; }

nb=${NANOBENCH_INCLUDE:-$root/test}
absl=${ABSL_ROOT:-/home/martinus/gra/abseil-install}
folly=${FOLLY_ROOT:-/home/martinus/gra/folly}
flags=(-O3 -DNDEBUG -std=c++20 -w -I"$build" -I"$nb" -I"$root/include" -I"$root/test"
       -I"$absl/include" -I"$folly" -I"${FOLLY_CONFIG:-/home/martinus/gra/folly-config}"
       -I"${EMHASH_INCLUDE:-/home/martinus/gra/emhash/include}"
       -I"${INDIVI_INCLUDE:-/home/martinus/gra/indivi_collection/calmsand/src}"
       -I"${VERSTABLE_INCLUDE:-/home/martinus/gra/Verstable/softwave}"
       -I"${IHTAB_INCLUDE:-/home/martinus/gra/ihtab/sololynx}")
[ "$keys" = str ] && flags+=(-DUDM_ONE_STR)
[ "$keys" = big ] && flags+=(-DUDM_ONE_BIG)
srcs=("$folly/folly/container/detail/F14Table.cpp" "$folly/folly/lang/SafeAssert.cpp" "$folly/folly/lang/ToAscii.cpp")
libs=(-Wl,--start-group "$absl"/lib64/libabsl_*.a -Wl,--end-group)
nbo="$build/nanobench_$(basename "$cxx").o"

# The map names, in the order maps.h lists them, so an index can be given a name.
mapfile -t names < <("$build/maps" names "$keys" 2>/dev/null || true)
[ ${#names[@]} -gt 0 ] || { echo "cannot ask $build/maps for its map names" >&2; exit 1; }

want=("$@")
printf '%-12s %10s %12s %12s %10s %10s %10s %10s\n' map ns/op instr cycles br-miss L1-miss dTLB-miss IPC
for i in "${!names[@]}"; do
    name=${names[$i]}
    if [ ${#want[@]} -gt 0 ]; then
        found=0
        for w in "${want[@]}"; do [ "$w" = "$name" ] && found=1; done
        [ $found = 1 ] || continue
    fi
    bin="$build/one_${keys}_${i}"
    "$cxx" "${flags[@]}" -DUDM_ONE_MAP="$i" "$root/scripts/ab/maps_one.cpp" "$nbo" "${srcs[@]}" "${libs[@]}" -o "$bin"
    # task-clock rather than an external timer: `/usr/bin/time -f %e` has 10 ms of resolution,
    # which over a run of a fifth of a second quantises ns/op into visible steps.
    pin=()
    [ -n "${AB_CORE:-}" ] && pin=(taskset -c "$AB_CORE")
    out=$(perf stat -x, -e task-clock,cycles,instructions,branch-misses,L1-dcache-load-misses,dTLB-load-misses \
              "${pin[@]}" "$bin" "$work" "$n" "$reps" 2>&1)
    get() { echo "$out" | awk -F, -v e="$1" '$3==e {print $1}'; }
    awk -v name="$name" -v reps="$reps" -v ms="$(get task-clock)" -v c="$(get cycles)" \
        -v ins="$(get instructions)" -v bm="$(get branch-misses)" \
        -v l1="$(get L1-dcache-load-misses)" -v tlb="$(get dTLB-load-misses)" \
        'BEGIN { printf "%-12s %10.2f %12.1f %12.1f %10.3f %10.3f %10.3f %10.2f\n",
                 name, ms*1e6/reps, ins/reps, c/reps, bm/reps, l1/reps, tlb/reps, ins/c }'
done
