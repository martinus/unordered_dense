#!/bin/bash
# Times prefetch_index() with both lines (as shipped), with only `p + 87`, and with only `p + 64`,
# one header per binary, every variant built by both compilers. See prefetch_index.cpp.
#
#   AB_BUILD=/home/martinus/gra/x AB_CORE=2 scripts/ab/prefetch_index.sh [rounds] [sizes...]
#
# Prints one line per (compiler, workload, size): the median ns per lookup of each variant and the
# ratio both/variant (above 1.00 means the variant is faster than what ships).
set -euo pipefail
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
rounds=${1:-5}
shift || true
sizes=("$@")
[ ${#sizes[@]} -gt 0 ] || sizes=(50000 200000 1000000 4000000 16000000)
variants=(both last first)

for v in "${variants[@]}"; do
    mkdir -p "$build/$v/ankerl"
    cp "$root/include/ankerl/stl.h" "$build/$v/ankerl/"
    python3 - "$root/include/ankerl/unordered_dense.h" "$build/$v/ankerl/unordered_dense.h" "$v" <<'PY'
import sys
src, dst, v = sys.argv[1:4]
s = open(src).read()
old = """        ANKERL_UNORDERED_DENSE_PREFETCH(p + first);
        if constexpr (second != first) {
            ANKERL_UNORDERED_DENSE_PREFETCH(p + second);
        }"""
if old not in s:
    sys.exit("prefetch_index() no longer looks the way this patch expects; update scripts/ab/prefetch_index.sh")
new = {"both": old,
       "last": "        (void)first;\n        ANKERL_UNORDERED_DENSE_PREFETCH(p + second);",
       "first": "        (void)second;\n        ANKERL_UNORDERED_DENSE_PREFETCH(p + first);"}[v]
open(dst, "w").write(s.replace(old, new, 1))
PY
    for cxx in clang++ g++; do
        "$cxx" -O3 -DNDEBUG -std=c++17 -w -I"$build/$v" -I"$root/test" \
            "$root/scripts/ab/prefetch_index.cpp" -o "$build/pi_${v}_${cxx}"
    done
done

for cxx in clang++ g++; do
    for work in hit64 hitstr miss64; do
        for n in "${sizes[@]}"; do
            declare -A res=()
            for r in $(seq 1 "$rounds"); do
                # rotate which variant runs first, so no side always sees the same machine state
                for k in 0 1 2; do
                    v=${variants[$(((r + k) % 3))]}
                    res[$v]+="$(${AB_CORE:+taskset -c $AB_CORE} "$build/pi_${v}_${cxx}" "$work" "$n") "
                done
            done
            python3 - "$cxx" "$work" "$n" "${res[both]}" "${res[last]}" "${res[first]}" <<'PY'
import statistics, sys
cxx, work, n, *cols = sys.argv[1:]
m = [statistics.median(float(x) for x in c.split()) for c in cols]
print(f"{cxx:8} {work:7} {int(n):>9}   both {m[0]:7.3f}  last {m[1]:7.3f}  first {m[2]:7.3f}"
      f"   both/last {m[0]/m[1]:.3f}  both/first {m[0]/m[2]:.3f}")
PY
            unset res
        done
    done
done
