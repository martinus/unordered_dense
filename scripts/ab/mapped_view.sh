#!/usr/bin/env bash
# A map_view over a mapped file against the owning map, on 4 KB and 2 MB pages (#301). One binary
# per mode; mapped_view.cpp says what each mode and each phase measures. The files go into AB_BUILD
# (on disk: a file mapping of tmpfs is not a file mapping).
#
#   AB_CORE=2 AB_BUILD=/home/martinus/gra/x scripts/ab/mapped_view.sh [-c clang++] [-n 1000000,4000000] [-r rounds] [-p warm,cold,shared] [-m owning,view_file]
#
# Prints `<compiler> <phase> <mode> <n> ...`. Smoke: -n 100000 -r 1.
set -euo pipefail
export LC_ALL=C
cxx=clang++ sizes=1000000,4000000,16000000,64000000 rounds=7 phases=warm,cold,shared
modes=owning,owning_huge,view_file,view_populate,view_collapse,view_thp_copy,view_hugetlb,header_file,header_populated,header_huge,header_file_checked
while getopts "c:n:r:p:m:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        n) sizes=$OPTARG ;;
        r) rounds=$OPTARG ;;
        p) phases=$OPTARG ;;
        m) modes=$OPTARG ;;
        *) exit 1 ;;
    esac
done
IFS=, read -r -a size_list <<<"$sizes"
IFS=, read -r -a phase_list <<<"$phases"
IFS=, read -r -a modes <<<"$modes"
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
mkdir -p "$build"
cc=$(basename "$cxx")
pin=(); [ -n "${AB_CORE:-}" ] && pin=(taskset -c "$AB_CORE")

nbo="$build/nanobench_$cc.o"
[ -f "$nbo" ] || (cd "$build" && printf '#define ANKERL_NANOBENCH_IMPLEMENT\n#include <third-party/nanobench.h>\n' > nb.cpp && "$cxx" -O2 -DNDEBUG -std=c++17 -w -I"$root/test" -c nb.cpp -o "$nbo")
for m in "${modes[@]}"; do
    "$cxx" -O3 -DNDEBUG -std=c++17 -w -I"$root/include" -I"$root/test" -DUDM_MODE_"$m"=1 \
        "$root/scripts/ab/mapped_view.cpp" "$nbo" -o "$build/mv_${m}_$cc" &
done
wait
echo "== $cc, THP $(cat /sys/kernel/mm/transparent_hugepage/enabled), nr_hugepages $(cat /proc/sys/vm/nr_hugepages)" >&2
for n in "${size_list[@]}"; do
    f="$build/map_$n.bin"
    [ -f "$f" ] || "$build/mv_${modes[0]}_$cc" gen "$f" "$n" >&2
    for p in "${phase_list[@]}"; do
        for m in "${modes[@]}"; do
            case $p in
                warm) echo "$cc warm $m $("${pin[@]}" "$build/mv_${m}_$cc" warm "$f" "$rounds")" ;;
                cold)
                    "$build/mv_${m}_$cc" warm "$f" 1 >/dev/null # the page cache holds the file
                    echo "$cc cold $m $("${pin[@]}" "$build/mv_${m}_$cc" cold "$f")"
                    echo "$cc cold_drop $m $("${pin[@]}" "$build/mv_${m}_$cc" cold "$f" drop)" ;;
                shared) echo "$cc shared $m $("$build/mv_${m}_$cc" shared "$f")" ;;
            esac
        done
    done
done
