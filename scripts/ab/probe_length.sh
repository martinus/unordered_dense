#!/bin/bash
# Groups visited per lookup, fresh and after churn: the drift a table takes on because nothing moves
# after it is placed, and what move_home() takes back.
#
#   scripts/ab/probe_length.sh [load] [turnovers] [writing hits per round]
#
# The counter cannot live in the header -- it would be in everybody's probe -- so this patches a
# copy of it, exactly the way run.sh makes its baseline copy, and builds against that. Nothing in
# the working tree is touched.
#
# `writing hits per round` is how many `operator[]` lookups on a present key each churn round does.
# That is the path move_home() runs on, so 0 measures the drift and 1 or 4 measure what taking it
# back is worth. Measured 2026-09-07 at load 0.799 over 200 turnovers, groups per miss: fresh 1.086,
# churned 1.122, and 1.081 and 1.052 with one and four writing hits -- so with enough writing
# lookups a churned table probes *better* than a freshly built one, because move_home() also pulls
# home the entries the original build left away from it.
set -euo pipefail
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"
cxx=${CXX_PROBE:-clang++}

# the probe, with a counter in it
python3 - "$root/include/ankerl/unordered_dense.h" "$build/instrumented.h" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
old = """        while (true) {
            prefetch_index(groups, group_idx);
            auto const& group = groups[group_idx];
            auto lanes = match_fingerprint(group, word);
            while (lanes != 0) {
                auto const lane = first_lane(lanes);
                auto const slot = static_cast<value_idx_type>(std::size_t{group_idx} * slots_per_group + lane);
                auto const value_idx = group.m_index[lane];"""
new = """        ++udm_probe_lookups;
        while (true) {
            ++udm_probe_groups;
            prefetch_index(groups, group_idx);
            auto const& group = groups[group_idx];
            auto lanes = match_fingerprint(group, word);
            while (lanes != 0) {
                auto const lane = first_lane(lanes);
                auto const slot = static_cast<value_idx_type>(std::size_t{group_idx} * slots_per_group + lane);
                auto const value_idx = group.m_index[lane];"""
if old not in s:
    sys.exit("probe() no longer looks the way this patch expects; update scripts/ab/probe_length.sh")
s = s.replace(old, new, 1)
s = s.replace("namespace ankerl::unordered_dense {",
              "extern unsigned long long udm_probe_groups;\nextern unsigned long long udm_probe_lookups;\n"
              "namespace ankerl::unordered_dense {", 1)
open(dst, "w").write(s)
PY

"$cxx" -O2 -DNDEBUG -std=c++17 -I"$build" -I"$root/include/ankerl" \
    -DUDM_INSTRUMENTED_HEADER='"instrumented.h"' "$root/scripts/ab/probe_length.cpp" -o "$build/probe_length"
exec "$build/probe_length" "$@"
