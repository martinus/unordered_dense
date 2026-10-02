#!/bin/bash
# Draws doc/allocated_memory.png: bytes held against time while four maps are filled.
#
#   scripts/ab/alloc_timeline.sh [-c compiler] [-l max-load-factor] [-n entries] [-u REV] [output-dir]
#
# -u takes this library's three headers from REV instead of the working tree, the same as
# bench_readme.sh -u, so the chart can show a release (`-u v5.2.0`). The version goes into the titles.
#
# -l applies to this library's maps only, since boost's setter is a no-op and abseil has none. It is
# a step function of the entry count rather than a dial: the bucket array is a power of two, so
# raising it saves an array only for the entry counts it moves across a power of two, and elsewhere
# it changes nothing but delays the doubling until the values are bigger, which makes the peak
# taller.
#
# Unlike everything else in this directory this one is *not* a timing benchmark -- what it plots is
# a byte count, which is exact -- but it is on the same clock, so run it on a quiet machine anyway:
# the x axis is wall time and a busy machine stretches it.
#
# boost and abseil are optional and are simply left off the chart when they are not installed;
# ABSL_ROOT points at an abseil that was built somewhere else. The output is the only thing under
# doc/ that is committed, because it is the one chart the README embeds.
set -euo pipefail
export LC_ALL=C
cxx=clang++ load=0 entries=10000000 udm_rev=
while getopts "c:l:n:u:" opt; do
    case $opt in
        c) cxx=$OPTARG ;;
        l) load=$OPTARG ;;
        n) entries=$OPTARG ;;
        u) udm_rev=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
out=${1:-$root/doc}
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build" "$out"

udm_inc=$root/include
if [ -n "$udm_rev" ]; then
    udm_inc=$build/udm_include
    mkdir -p "$udm_inc/ankerl"
    for h in unordered_dense.h stl.h huge_page_allocator.h; do
        git -C "$root" show "$udm_rev:include/ankerl/$h" > "$udm_inc/ankerl/$h"
    done
fi
udm_version=$(awk '/define ANKERL_UNORDERED_DENSE_VERSION_(MAJOR|MINOR|PATCH)/ {v = v (v ? "." : "") $3} END {print v}' "$udm_inc/ankerl/unordered_dense.h")
echo "== unordered_dense $udm_version from ${udm_rev:-the working tree}" >&2

flags=(-O2 -DNDEBUG -std=c++17 -w -I"$udm_inc")
libs=()
absl=${ABSL_ROOT:-/home/martinus/gra/abseil-install}
if [ -f "$absl/include/absl/container/flat_hash_map.h" ]; then
    flags+=(-I"$absl/include")
    libs+=(-Wl,--start-group "$absl"/lib64/libabsl_*.a -Wl,--end-group)
fi

"$cxx" "${flags[@]}" "$root/scripts/ab/alloc_timeline.cpp" "${libs[@]}" -o "$build/alloc_timeline"
# A series left over from an earlier run on a machine that had boost would otherwise be replotted
# beside today's measurements.
rm -f "$out"/allocated_memory_*.csv
"$build/alloc_timeline" "$out" "$load" "$entries"

# Only the series that were actually produced go to gnuplot, so a machine without boost or abseil
# gets a chart of what it could measure rather than an error about a missing file.
files=() titles=()
add() { [ -f "$out/allocated_memory_$1.csv" ] && files+=("allocated_memory_$1.csv") && titles+=("$2") || true; }
add map "\"ankerl::unordered_dense::map $udm_version\""
add segmented_map "\"ankerl::unordered_dense::segmented_map $udm_version\""
add boost_flat_map boost::unordered_flat_map
add absl_flat_hash_map absl::flat_hash_map
(cd "$out" && gnuplot -e "files='${files[*]}'; titles='${titles[*]}'" "$root/doc/allocated_memory.gnuplot")
echo "wrote $out/allocated_memory.png"
