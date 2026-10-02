#!/usr/bin/env bash
# Loading a map from its values and its index (#299), one binary per mode, each running its rounds
# back to back and printing the median. index_load.cpp says what each mode times.
#
#   AB_CORE=2 AB_BUILD=/home/martinus/gra/x scripts/ab/index_load.sh [-c clang++] [-k u64|str] [-n 1000000,4000000] [-r rounds]
#
# Prints `<compiler> <key> <mode> <n> ns/entry <median> min <min> max <max> rss_B/entry <bytes>`.
# Smoke: -n 10000 -r 1.
set -euo pipefail
export LC_ALL=C
cxx=clang++ key=u64 sizes=1000000,4000000,16000000,64000000 rounds=7
while getopts "c:k:n:r:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        k) key=$OPTARG ;;
        n) sizes=$OPTARG ;;
        r) rounds=$OPTARG ;;
        *) exit 1 ;;
    esac
done
IFS=, read -r -a size_list <<<"$sizes"
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
mkdir -p "$build"

modes=(build owning owning_unchecked view_checked view_unchecked verify_spot verify_full)
kflag=(); [ "$key" = str ] && kflag=(-DUDM_STR)
nbo="$build/nanobench_$(basename "$cxx").o"
[ -f "$nbo" ] || (cd "$build" && printf '#define ANKERL_NANOBENCH_IMPLEMENT\n#include <third-party/nanobench.h>\n' > nb.cpp && "$cxx" -O2 -DNDEBUG -std=c++17 -w -I"$root/test" -c nb.cpp -o "$nbo")
for m in "${modes[@]}"; do
    "$cxx" -O3 -DNDEBUG -std=c++17 -w -I"$root/include" -I"$root/test" "${kflag[@]}" -DUDM_MODE_"$m"=1 \
        "$root/scripts/ab/index_load.cpp" "$nbo" -o "$build/load_${key}_${m}_$(basename "$cxx")" &
done
wait
echo "== $(basename "$cxx") $key, THP $(cat /sys/kernel/mm/transparent_hugepage/enabled)" >&2
for n in "${size_list[@]}"; do
    for m in "${modes[@]}"; do
        echo "$(basename "$cxx") $key $m $(${AB_CORE:+taskset -c $AB_CORE} "$build/load_${key}_${m}_$(basename "$cxx")" "$n" "$rounds")"
    done
done
