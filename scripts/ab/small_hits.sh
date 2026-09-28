#!/usr/bin/env bash
# small_hits.cpp for several revisions of the header, both compilers, one binary each, ROUNDS rounds
# with the binaries rotated, medians per cell as ratios to the first revision (#346). Pinned with
# AB_CORE (default 2).
#
#   AB_BUILD=/home/martinus/gra/x scripts/ab/small_hits.sh [ROUNDS] [REV...]     # default v4.5.0 origin/main
#
# REV "wt" is the working tree's header.
set -euo pipefail
rounds=${1:-3}
shift || true
revs=("$@")
[ ${#revs[@]} -gt 0 ] || revs=(v4.5.0 origin/main)
root=$(git rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
core=${AB_CORE:-2}
for rev in "${revs[@]}"; do
    d="$build/hdr-${rev//\//_}/ankerl"
    mkdir -p "$d"
    for f in unordered_dense.h stl.h; do
        if [ "$rev" = wt ]; then
            cp "$root/include/ankerl/$f" "$d/$f"
        else
            git -C "$root" show "$rev:include/ankerl/$f" >"$d/$f" 2>/dev/null || rm -f "$d/$f"
        fi
    done
done
for cxx in clang++ g++; do
    for rev in "${revs[@]}"; do
        "$cxx" -O3 -DNDEBUG -std=c++17 -I"$build/hdr-${rev//\//_}" -I"$root/test" \
            "$root/scripts/ab/small_hits.cpp" -o "$build/$cxx-${rev//\//_}" &
    done
done
wait
raw="$build/raw.txt"
: >"$raw"
for ((r = 0; r < rounds; ++r)); do
    for cxx in clang++ g++; do
        for ((i = 0; i < ${#revs[@]}; ++i)); do
            rev=${revs[$(((i + r) % ${#revs[@]}))]}
            taskset -c "$core" "$build/$cxx-${rev//\//_}" | sed "s|^|$cxx $rev |" >>"$raw"
        done
    done
done
python3 - "$raw" "${revs[@]}" <<'PY'
import sys, statistics, collections
raw, revs = sys.argv[1], sys.argv[2:]
d = collections.defaultdict(list)
for line in open(raw):
    cxx, rev, key, n, mode, ns = line.split()
    d[(cxx, key, int(n), mode, rev)].append(float(ns))
print("compiler key n mode " + " ".join(f"{r}_ns" for r in revs) + " " + " ".join(f"{r}/{revs[0]}" for r in revs[1:]))
for cxx in ("clang++", "g++"):
    for key in ("uint64", "seq", "string"):
        for n in (1000, 4000, 16000):
            for mode in ("quiet", "busy"):
                m = [statistics.median(d[(cxx, key, n, mode, r)]) for r in revs]
                print(cxx, key, n, mode, " ".join(f"{x:.2f}" for x in m), " ".join(f"{x / m[0]:.3f}" for x in m[1:]))
PY
