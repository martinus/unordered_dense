#!/bin/bash
# One binary, run alternately with and without an environment setting; the score's ratio per round.
#
#   scripts/ab/same_binary.sh [-t TESTCASE] [-b BINARY] [rounds] VAR=value [VAR=value...]
#
# For anything the paired harness cannot see because both of its sides share one process: the page
# size (GLIBC_TUNABLES=glibc.malloc.hugetlb=1), an allocator tunable, a CPU governor. There is no
# second build and no code layout to argue with, so the ratio is the environment and nothing else.
# Above 1.00 means the setting is faster. Keep the machine otherwise idle; AB_CORE pins.
set -euo pipefail
export LC_ALL=C
tc=bench_quick_overall_udm bin=builddir/clang_release/test/udm-test
while getopts "t:b:" opt; do
    case $opt in
        t) tc=$OPTARG ;;
        b) bin=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
rounds=5
[[ ${1:-} =~ ^[0-9]+$ ]] && { rounds=$1; shift; }
[ $# -ge 1 ] || { sed -n '2,10p' "$0"; exit 1; }
score() { ${AB_CORE:+taskset -c $AB_CORE} "$bin" -ns -tc="$tc" 2>&1 | grep bench_quick_overall_map_udm | awk '{print $1}'; }
echo "$bin, $tc, plain against: $*"
sum=0
for r in $(seq 1 "$rounds"); do
    a=$(score)
    b=$(env "$@" bash -c "$(declare -f score); bin='$bin'; tc='$tc'; score")
    ratio=$(awk -v a="$a" -v b="$b" 'BEGIN {printf "%.4f", a / b}')
    sum=$(awk -v s="$sum" -v r="$ratio" 'BEGIN {print s + log(r)}')
    echo "  round $r   plain $a   with $b   ratio $ratio"
done
awk -v s="$sum" -v n="$rounds" 'BEGIN {printf "  geomean %.4f  (above 1.00 means the setting is faster)\n", exp(s / n)}'
