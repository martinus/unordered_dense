#!/bin/bash
# place_element_at() with its always_inline against without, one map per translation unit, both
# compilers, rotated rounds, median of each round's best build; plus perf instruction counts.
#
#   AB_BUILD=/home/martinus/gra/x AB_CORE=2 scripts/ab/place_inline.sh [rounds] [sizes...]
#
# Prints, per (compiler, key, size), ns per insert with / without and without/with (above 1.00
# means the attribute makes the build faster), and instructions per insert both ways.
set -euo pipefail
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
rounds=${1:-5}
shift || true
sizes=("$@")
[ ${#sizes[@]} -gt 0 ] || sizes=(32000 200000 1000000)

for v in with without; do
    mkdir -p "$build/$v/ankerl"
    cp "$root/include/ankerl/stl.h" "$build/$v/ankerl/"
    python3 - "$root/include/ankerl/unordered_dense.h" "$build/$v/ankerl/unordered_dense.h" "$v" <<'PY'
import sys
src, dst, v = sys.argv[1:4]
s = open(src).read()
old = """    template <typename... Args>
    ANKERL_UNORDERED_DENSE_FORCEINLINE auto
    place_element_at("""
if s.count(old) != 1:
    sys.exit("place_element_at() no longer looks the way this patch expects; update scripts/ab/place_inline.sh")
if v == "without":
    s = s.replace(old, old.replace("ANKERL_UNORDERED_DENSE_FORCEINLINE", "inline"), 1)
open(dst, "w").write(s)
PY
    for cxx in clang++ g++; do
        "$cxx" -O3 -DNDEBUG -std=c++17 -w -I"$build/$v" -I"$root/test" \
            "$root/scripts/ab/place_inline.cpp" -o "$build/pl_${v}_${cxx}"
    done
done

pin=(); [ -n "${AB_CORE:-}" ] && pin=(taskset -c "$AB_CORE")
for cxx in clang++ g++; do
    for key in u64 str; do
        for n in "${sizes[@]}"; do
            w=() wo=()
            for r in $(seq 1 "$rounds"); do
                if (( r % 2 )); then
                    w+=("$("${pin[@]}" "$build/pl_with_${cxx}" "$key" "$n")"); wo+=("$("${pin[@]}" "$build/pl_without_${cxx}" "$key" "$n")")
                else
                    wo+=("$("${pin[@]}" "$build/pl_without_${cxx}" "$key" "$n")"); w+=("$("${pin[@]}" "$build/pl_with_${cxx}" "$key" "$n")")
                fi
            done
            # instructions per insert: one build each, net of a zero-size run's fixed cost
            ins() { "${pin[@]}" perf stat -x, -e instructions "$build/pl_${1}_${cxx}" "$key" "$n" 1 2>&1 >/dev/null | awk -F, '/instructions/{print $1}'; }
            ins0() { "${pin[@]}" perf stat -x, -e instructions "$build/pl_${1}_${cxx}" "$key" 1 1 2>&1 >/dev/null | awk -F, '/instructions/{print $1}'; }
            python3 - "$cxx" "$key" "$n" "${w[*]}" "${wo[*]}" "$(ins with)" "$(ins0 with)" "$(ins without)" "$(ins0 without)" <<'PY'
import statistics, sys
cxx, key, n, w, wo, iw, iw0, iwo, iwo0 = sys.argv[1:]
n = int(n)
mw = statistics.median(map(float, w.split())); mwo = statistics.median(map(float, wo.split()))
# two builds ran (warm-up + one), so the per-insert count is over 2n inserts
pw = (int(iw) - int(iw0)) / (2 * n); pwo = (int(iwo) - int(iwo0)) / (2 * n)
print(f"{cxx:8} {key:4} {n:>8}   ns/insert with {mw:7.2f} without {mwo:7.2f}  without/with {mwo/mw:.3f}"
      f"   ins/insert with {pw:6.1f} without {pwo:6.1f}  ({pwo/pw:.3f})")
PY
        done
    done
done
