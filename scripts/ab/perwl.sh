#!/bin/bash
# Per-workload ratios of one nanobench column from the two binaries of a solo.sh build directory.
#
#   scripts/ab/perwl.sh [-m ins|ns|cyc|bra|miss] [-t TESTCASE] <AB_BUILD dir>
#
# Prints candidate/baseline per scored workload. The default column is ins/op, which neither code
# layout nor drift can move: two runs of the same two binaries give the same fifteen numbers to
# four decimals, so a difference is the change or the inliner and never noise -- and a workload
# that *cannot* execute the changed code and still moves is the inliner (#254, #268). Below 1.00
# means the candidate does less. This is the measurement that decides anything under 10%.
set -euo pipefail
export LC_ALL=C
metric=ins tc=bench_quick_overall_udm
while getopts "m:t:" opt; do
    case $opt in
        m) metric=$OPTARG ;;
        t) tc=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
d=${1:?AB_BUILD dir with udm-base and udm-cand}
case $metric in
    ns) col=2 ;; ins) col=5 ;; cyc) col=6 ;; bra) col=8 ;; miss) col=9 ;;
    *) echo "metric must be ins, ns, cyc, bra or miss" >&2; exit 1 ;;
esac
extract() {
    ${AB_CORE:+taskset -c $AB_CORE} "$1" -ns -tc="$tc" 2>/dev/null |
        awk -F'|' -v c="$col" '/ankerl::unordered_dense::map</ {
            v = $c; gsub(/[ ,%]/, "", v); n = $11; gsub(/^ +| +$|`/, "", n)
            sub(/ankerl::unordered_dense::map/, "", n); print n "\t" v }'
}
paste <(extract "$d/udm-base") <(extract "$d/udm-cand") |
    awk -F'\t' -v m="$metric" '{printf "  %8.4f  %s\n", $4 / $2, $1} END {print "  (candidate over baseline, " m "/op)"}'
