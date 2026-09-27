#!/bin/bash
# The caller corpus (scripts/ab/caller_corpus.cpp) for several headers: every loop, both compilers,
# in cache and past it, one binary per loop, header and compiler.
#
#   scripts/ab/caller_corpus.sh [-c "clang++ g++"] [-n "65536 4194304"] [-r ROUNDS] HEADER...
#
# HEADER is a git revision (its include/ankerl/{unordered_dense,stl}.h) or a path to an
# unordered_dense.h, next to which a stl.h is expected. Prints cycles per operation for every cell,
# each header's ratio to the fastest header in that cell, and each header's worst ratio: the
# number an inlining or insert-shape change has to be judged by, because the loops here fall into
# the spill trap (notes, "stored and reloaded") or not depending on how much of the map is inlined
# into them, and a change that wins the score can lose 2x in one of them.
#
# AB_BUILD is the build directory (on disk, not /tmp), AB_CORE the core to pin to (default 2).
# Instruction counts are deterministic; cycles are the median of ROUNDS (default 3) rounds, each
# round running every header once per cell, so drift lands on all of them alike.
set -euo pipefail

compilers="clang++ g++"
sizes="65536 4194304"
rounds=3
while getopts "c:n:r:" opt; do
    case $opt in
        c) compilers=$OPTARG ;;
        n) sizes=$OPTARG ;;
        r) rounds=$OPTARG ;;
        *) exit 1 ;;
    esac
done
shift $((OPTIND - 1))
[ $# -ge 1 ] || { echo "usage: $0 [-c compilers] [-n sizes] [-r rounds] HEADER..." >&2; exit 1; }

root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
core=${AB_CORE:-2}
loops="udb3_insert udb3_del bump_mod bump_mask churn_mod build string_hit struct_key mysql_except"
mkdir -p "$build"

names=()
i=0
for h in "$@"; do
    inc="$build/h$i/ankerl"
    mkdir -p "$inc"
    if [ -f "$h" ]; then
        cp "$h" "$inc/unordered_dense.h"
        cp "$(dirname "$h")/stl.h" "$inc/stl.h"
    else
        git -C "$root" show "$h:include/ankerl/unordered_dense.h" > "$inc/unordered_dense.h"
        git -C "$root" show "$h:include/ankerl/stl.h" > "$inc/stl.h"
    fi
    names+=("$h")
    i=$((i + 1))
done
nh=$i

# Every binary first, so that no compile lands between two timed runs.
pids=()
for cxx in $compilers; do
    for l in $loops; do
        for ((h = 0; h < nh; h++)); do
            "$cxx" -O3 -DNDEBUG -std=c++17 -DLOOP_$l -I"$build/h$h" "$root/scripts/ab/caller_corpus.cpp" \
                -o "$build/$(basename "$cxx")_${l}_$h" &
            pids+=($!)
        done
    done
done
for p in "${pids[@]}"; do wait "$p"; done

raw="$build/raw.txt"
: > "$raw"
for ((r = 0; r < rounds; r++)); do
    for cxx in $compilers; do
        for l in $loops; do
            for n in $sizes; do
                for ((h = 0; h < nh; h++)); do
                    line=$(taskset -c "$core" "$build/$(basename "$cxx")_${l}_$h" "$n")
                    echo "$(basename "$cxx") $h $line" >> "$raw"
                done
            done
        done
    done
done

python3 - "$raw" "${names[@]}" <<'PY'
import collections, statistics, sys
raw, names = sys.argv[1], sys.argv[2:]
cyc = collections.defaultdict(list)
ins = {}
for line in open(raw):
    cxx, h, loop, n, i, c = line.split()
    cyc[(cxx, loop, int(n), int(h))].append(float(c))
    ins[(cxx, loop, int(n), int(h))] = float(i)
cells = sorted({(k[0], k[1], k[2]) for k in cyc}, key=lambda k: (k[0], k[1], k[2]))
worst = collections.defaultdict(float)
print("cycles per operation (instructions), and the ratio to the fastest header in the cell")
print(f"{'compiler':8} {'loop':13} {'n':>8}  " + "  ".join(f"{str(i) + ': ' + nm[:18]:>32}" for i, nm in enumerate(names)))
for cxx, loop, n in cells:
    med = [statistics.median(cyc[(cxx, loop, n, h)]) for h in range(len(names))]
    best = min(med)
    row = []
    for h in range(len(names)):
        ratio = med[h] / best
        worst[(cxx, h)] = max(worst[(cxx, h)], ratio)
        row.append(f"{med[h]:9.1f} ({ins[(cxx, loop, n, h)]:6.1f}) {ratio:5.2f}")
    print(f"{cxx:8} {loop:13} {n:>8}  " + "  ".join(f"{x:>32}" for x in row))
print()
for cxx in sorted({k[0] for k in cells}):
    print(f"worst ratio, {cxx}: " + "  ".join(f"{i}: {worst[(cxx, i)]:.2f}" for i in range(len(names))))
PY
