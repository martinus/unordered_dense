#!/bin/bash
# Runs prefetch_lines.cpp's modes against each other, interleaved round by round, and reports the
# median of each. Interleaving is the point: the machine drifts several percent over minutes, and
# the modes are compared within one binary because each one is its own template instantiation and a
# build's layout moves everything by a percent or two.
#
#   scripts/ab/prefetch_lines.sh [-c COMPILER] [-k group|big] [-s probe|full] [-n BLOCKS] [-r REPS]
#                                [rounds] [mode...]
#
# Defaults are a 304 MiB array of `group_big` blocks read the way a lookup reads one, seven rounds,
# and the four modes #252 chose between. The array has to be far larger than the last level cache
# for the question to mean anything; -n 50000 asks it inside the cache instead.
set -euo pipefail
export LC_ALL=C
cxx=clang++ which=big shape=probe blocks=2000000 reps=20000000
while getopts "c:k:s:n:r:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        k) which=$OPTARG ;;
        s) shape=$OPTARG ;;
        n) blocks=$OPTARG ;;
        r) reps=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
rounds=${1:-7}
[ $# -gt 0 ] && shift
modes=("$@")
[ ${#modes[@]} -gt 0 ] || modes=(pair step steptail stepfull)

root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"
bin="$build/prefetch_lines_$(basename "$cxx")"
"$cxx" -O3 -DNDEBUG -std=c++17 -I"$root/include" -I"$root/test" -o "$bin" "$root/scripts/ab/prefetch_lines.cpp"

declare -A runs
for _ in $(seq 1 "$rounds"); do
    for m in "${modes[@]}"; do
        runs[$m]+="$(${AB_CORE:+taskset -c $AB_CORE} "$bin" "$m" "$shape" "$blocks" "$reps" "$which" | awk '{print $1}') "
    done
done

echo "$cxx, $which blocks, $shape reads, $blocks blocks, $rounds rounds"
for m in "${modes[@]}"; do
    printf '  %-9s ' "$m"
    tr ' ' '\n' <<<"${runs[$m]}" | grep -v '^$' | sort -n |
        awk '{a[NR] = $1} END {printf "median %7.3f   min %7.3f   max %7.3f\n",
              (NR % 2 ? a[(NR + 1) / 2] : (a[NR / 2] + a[NR / 2 + 1]) / 2), a[1], a[NR]}'
done
