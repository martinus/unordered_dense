#!/usr/bin/env bash
# small_maps.cpp for several header variants, both compilers, both key types, rounds alternated
# between the variants, the median per cell printed at the end (#304). Pinned with AB_CORE
# (default 2).
#
#   AB_BUILD=/home/martinus/gra/x scripts/ab/small_maps.sh [rounds] name=header_dir ...
#
# Each header_dir holds ankerl/unordered_dense.h and ankerl/stl.h. Prints:
#   compiler variant key size build_ns lookup_ns destroy_ns index_bytes
set -euo pipefail
rounds=${1:?rounds}
shift
root=$(git rev-parse --show-toplevel)
build=${AB_BUILD:?set AB_BUILD to a directory on disk, not /tmp}
core=${AB_CORE:-2}
variants=("$@")
for cxx in clang++ g++; do
    for v in "${variants[@]}"; do
        name=${v%%=*} dir=${v#*=}
        for key in INT STRING_KEY; do
            "$cxx" -O3 -DNDEBUG -std=c++17 -I"$dir" -D$key "$root/scripts/ab/small_maps.cpp" \
                -o "$build/$cxx-$name-$key" &
        done
    done
done
wait
raw="$build/raw.txt"
: >"$raw"
for ((i = 0; i < rounds; ++i)); do
    for cxx in clang++ g++; do
        for key in INT STRING_KEY; do
            for v in "${variants[@]}"; do
                name=${v%%=*}
                taskset -c "$core" "$build/$cxx-$name-$key" | sed "s/^/$cxx $name /" >>"$raw"
            done
        done
    done
done
python3 - "$raw" <<'PY'
import sys, statistics, collections
cells = collections.defaultdict(list)
order = []
for line in open(sys.argv[1]):
    c, v, k, n, b, l, d, ib = line.split()
    key = (c, k, int(n), v)
    if key not in cells:
        order.append(key)
    cells[key].append((float(b), float(l), float(d), float(ib)))
print("compiler key size variant build_ns lookup_ns destroy_ns index_bytes")
for key in sorted(order, key=lambda t: (t[0], t[1], t[2])):
    rows = cells[key]
    med = [statistics.median(r[i] for r in rows) for i in range(4)]
    print(*key[:3], key[3], *("%.1f" % m for m in med[:3]), "%.0f" % med[3])
PY
