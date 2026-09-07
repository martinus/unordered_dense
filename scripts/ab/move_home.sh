#!/bin/bash
# Build scripts/ab/move_home.cpp twice, with move_home() on and turned into a no-op, and run both.
# One map per binary, because this is a 10% question about a path that is not on the scored suite.
#
#   scripts/ab/move_home.sh <miss|hit|round> <entries> <turnovers> <writing hits per round> <reps>
#
# The switch does not live in the header -- it would be a knob nobody should turn -- so this patches
# a copy of it, the way run.sh copies the baseline. Nothing in the working tree is touched.
set -euo pipefail
[ $# -ge 5 ] || { sed -n '2,8p' "$0"; exit 1; }
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build/on" "$build/off"
cxx=${CXX_MH:-clang++}

cp "$root/include/ankerl/unordered_dense.h" "$build/on/unordered_dense.h"
python3 - "$root/include/ankerl/unordered_dense.h" "$build/off/unordered_dense.h" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
old = """    void move_home(value_idx_type slot, std::uint64_t mh) {
        auto const found_in"""
new = """    void move_home(value_idx_type slot, std::uint64_t mh) {
        (void)slot;
        (void)mh;
        return;
        auto const found_in"""
if old not in s:
    sys.exit("move_home() no longer looks the way this patch expects; update scripts/ab/move_home.sh")
open(dst, "w").write(s.replace(old, new, 1))
PY

for v in on off; do
    mkdir -p "$build/$v/ankerl"
    cp "$build/$v/unordered_dense.h" "$build/$v/ankerl/unordered_dense.h"
    cp "$root/include/ankerl/stl.h" "$build/$v/ankerl/" 2>/dev/null || true
    "$cxx" -O3 -DNDEBUG -std=c++17 -w -I"$build/$v" -I"$build/$v/ankerl" \
        "$root/scripts/ab/move_home.cpp" -o "$build/mh_$v"
done
printf '  %-10s ' move_home; "$build/mh_on" "$@"
printf '  %-10s ' off;       "$build/mh_off" "$@"
