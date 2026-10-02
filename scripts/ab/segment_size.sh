#!/usr/bin/env bash
# segmented_map's segment size against the page it lands on (#350). One binary per variant
# (segment_size.cpp says which macros pick it), rounds interleaved over every variant, one line per
# (variant, workload, base, round) on stdout:
#
#   <kind> <variant> <workload> <base> <five octave points> geomean <g>
#
# kind: u64 (16 byte pair), str (40), big (72). variant: map, huge-map, seg<bytes>, huge-seg<bytes>,
# exact4096 (segment_exact_page.patch: one aligned page per segment, division indexing).
#
#   AB_CORE=2 AB_BUILD=/home/martinus/gra/x scripts/ab/segment_size.sh \
#       [-c clang++] [-k u64,str,big] [-b 50000,200000,1000000,4000000] [-w buildfree,find,churn,iterate,rss]
#       [-r rounds] [-o ops per cell] [-v variant regex]
#
# Smoke: -k u64 -b 1000 -r 1 -o 10000 -v 'map|seg4096|exact'. The full grid (45 binaries, 4 bases,
# 5 workloads) does not fit one hour: split it by -k and -b.
set -euo pipefail
export LC_ALL=C
cxx=clang++ kinds=u64,str,big bases=50000,200000,1000000,4000000 works=buildfree,find,churn,iterate,rss rounds=1
ops=3000000 vfilter=.
while getopts "c:k:b:w:r:o:v:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        k) kinds=$OPTARG ;;
        b) bases=$OPTARG ;;
        w) works=$OPTARG ;;
        r) rounds=$OPTARG ;;
        o) ops=$OPTARG ;;
        v) vfilter=$OPTARG ;;
        *) exit 1 ;;
    esac
done
IFS=, read -r -a kind_list <<<"$kinds"
IFS=, read -r -a base_list <<<"$bases"
IFS=, read -r -a work_list <<<"$works"

root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
mkdir -p "$build/exact/ankerl"
git -C "$root" show v4.11.0:include/ankerl/unordered_dense.h |
    sed "s/ankerl::unordered_dense/udmbase::unordered_dense/g; s/ANKERL_UNORDERED_DENSE/UDMBASE_UNORDERED_DENSE/g; s/namespace ankerl/namespace udmbase/g; s|#        include \"stl.h\"|#        include <ankerl/stl.h>|" > "$build/base411.h"
cp "$root/include/ankerl/"*.h "$build/exact/ankerl/"
patch -s -d "$build/exact" -p1 < <(sed 's|include/ankerl/|ankerl/|' "$root/scripts/ab/segment_exact_page.patch")

common=(-O3 -DNDEBUG -std=c++20 -w -DUDM_NO_ABSL -I"$build" -I"$root/test")
nbo="$build/nanobench_$(basename "$cxx").o"
[ -f "$nbo" ] || (cd "$build" && printf '#define ANKERL_NANOBENCH_IMPLEMENT\n#include <third-party/nanobench.h>\n' > nb.cpp && "$cxx" -O2 -DNDEBUG -std=c++20 -w -I"$root/test" -c nb.cpp -o "$nbo")

declare -A kind_id=([u64]=0 [str]=1 [big]=2)
variants=()
for k in "${kind_list[@]}"; do
    for v in map huge-map seg4096 seg16384 seg65536 seg262144 seg2097152 seg16777216 \
        huge-seg4096 huge-seg16384 huge-seg65536 huge-seg262144 huge-seg2097152 huge-seg16777216 exact4096; do
        grep -Eq "$vfilter" <<<"$v" && variants+=("$k/$v")
    done
done

pids=()
for kv in "${variants[@]}"; do
    k=${kv%/*} v=${kv#*/}
    bytes=0 huge=0 inc=$root/include
    case $v in
        huge-map) huge=1 ;;
        huge-seg*) huge=1 bytes=${v#huge-seg} ;;
        seg*) bytes=${v#seg} ;;
        exact*) bytes=${v#exact} inc=$build/exact ;;
    esac
    bin="$build/seg_${k}_${v}_$(basename "$cxx")"
    "$cxx" "${common[@]}" -I"$inc" -DUDM_SEG_KIND="${kind_id[$k]}" -DUDM_SEG_BYTES="$bytes" -DUDM_SEG_HUGE="$huge" \
        "$root/scripts/ab/segment_size.cpp" "$nbo" -o "$bin" &
    pids+=($!)
    if [ ${#pids[@]} -ge 16 ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
done
for p in "${pids[@]}"; do wait "$p"; done
echo "== $(basename "$cxx"), ${#variants[@]} variants, THP $(cat /sys/kernel/mm/transparent_hugepage/enabled)" >&2

for ((r = 1; r <= rounds; r++)); do
    for b in "${base_list[@]}"; do
        for w in "${work_list[@]}"; do
            for kv in "${variants[@]}"; do
                k=${kv%/*} v=${kv#*/}
                line=$(${AB_CORE:+taskset -c $AB_CORE} "$build/seg_${k}_${v}_$(basename "$cxx")" "$w" "$b" "$ops")
                echo "$k $v ${line}"
            done
        done
    done
done
