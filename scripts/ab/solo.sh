#!/bin/bash
# One header per binary: build the scored benchmark twice, from two revisions of the header, and
# alternate whole runs.
#
#   scripts/ab/solo.sh [-r REV] [-c COMPILER] [rounds]
#
# The paired harness next door interleaves baseline and candidate epoch by epoch in one process,
# which cancels drift and is the right tool for almost everything. It cannot measure a change that
# alters *inlining*: it compiles both headers into one translation unit, which is exactly the
# condition under which a compiler exhausts its inlining budget, so shrinking one header changes
# what is inlined in both and the ratio reports that instead of the change. Measured 2026-09-08 on
# removing do_place_element's force-inline, it read 0.979 under clang and 0.995 under gcc with a
# clean control, where this script reads 1.017 and 1.039 -- a sign reversal on both compilers.
#
# What this one cannot do is cancel drift, and the two binaries have different code layouts, which
# alternating does not remove either. So: use run.sh by default, use this for anything touching an
# always_inline, a function's size or a template's instantiation boundary, and settle anything under
# 10% with instruction counts from maps_one.sh, which neither layout nor drift can move.
set -euo pipefail
export LC_ALL=C
rev=HEAD cxx=clang++
while getopts "r:c:" opt; do
    case $opt in
        r) rev=$OPTARG ;;
        c) cxx=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
rounds=${1:-3}
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}

# Two copies of the source that differ only in the header, each with its own build directory.
# Putting the baseline header on an -I in cpp_args does not work: meson emits the project's own
# -I../../include ahead of cpp_args, so the tree's header wins and both binaries come out
# identical -- which reads as a very convincing 1.00.
for side in base cand; do
    src="$build/src_$side"
    [ -d "$src" ] || { mkdir -p "$src"; tar -C "$root" --exclude=./builddir --exclude=./.git \
        --exclude=./subprojects/packagecache -cf - . | tar -C "$src" -xf -; }
done
git -C "$root" show "$rev:include/ankerl/unordered_dense.h" > "$build/src_base/include/ankerl/unordered_dense.h"
cp "$root/include/ankerl/unordered_dense.h" "$build/src_cand/include/ankerl/unordered_dense.h"
cmp -s "$build/src_base/include/ankerl/unordered_dense.h" "$build/src_cand/include/ankerl/unordered_dense.h" &&
    echo "  note: the headers are identical, so this measures code layout and nothing else"

for side in base cand; do
    dir="$build/src_$side/bd_$(basename "$cxx")"
    [ -f "$dir/build.ninja" ] || CXX="$cxx" meson setup --buildtype release "$dir" "$build/src_$side" >/dev/null
    ninja -C "$dir" test/udm-test >/dev/null
    cp "$dir/test/udm-test" "$build/udm-$side"
done

score() { "$1" -ns -tc=bench_quick_overall_udm 2>&1 | grep bench_quick_overall_map_udm | awk '{print $1}'; }
echo "one header per binary, $cxx, candidate = working tree, baseline = $rev"
for r in $(seq 1 "$rounds"); do
    b=$(score "$build/udm-base"); c=$(score "$build/udm-cand")
    printf "  round %d   baseline %.6f   candidate %.6f   ratio %.4f\n" "$r" "$b" "$c" \
        "$(awk -v b="$b" -v c="$c" 'BEGIN{print b/c}')"
    echo "$b $c" >> "$build/rounds.txt"
done
awk '{b+=log($1); c+=log($2); n++} END {printf "  median-free geomean ratio %.4f  (above 1.00 means the working tree is faster)\n", exp((b-c)/n)}' "$build/rounds.txt"
