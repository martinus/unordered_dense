#!/bin/bash
# Groups visited per lookup, fresh and after churn: the drift a table takes on because nothing moves
# after it is placed, and what move_home() takes back.
#
#   scripts/ab/probe_length.sh [load] [turnovers] [writing hits per round] [groups, default 4096]
#
# The counter cannot live in the header -- it would be in everybody's probe -- so this patches a
# copy of it, exactly the way run.sh makes its baseline copy, and builds against that. Nothing in
# the working tree is touched. AB_HEADER=<path> instruments that header instead of the working tree's
# (scripts/ab/probe_sequence.sh passes each probe sequence variant this way).
#
# `writing hits per round` is how many `operator[]` lookups on a present key each churn round does.
# That is the path move_home() runs on, so 0 measures the drift and 1 or 4 measure what taking it
# back is worth. At load 0.76 over 200 turnovers (2026-10-02), groups per hit / per miss: churned
# 1.136 / 1.265, and 1.094 / 1.163 and 1.056 / 1.099 with one and four writing hits. The figures
# this comment carried before (churned 1.122 per miss at 0.799, and a churned table probing
# *better* than a fresh one) came from churning in sequential keys, which drift far less.
set -euo pipefail
root=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
build=${AB_BUILD:-$(mktemp -d)}
mkdir -p "$build"
cxx=${CXX_PROBE:-clang++}

# the probe, with a counter in it
python3 - "${AB_HEADER:-$root/include/ankerl/unordered_dense.h}" "$build/instrumented.h" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src).read()
# Groups are counted in probe_from's loop and lookups in probe(). For an integer key probe() is
# probe_from from the home group on, so the ratio is groups per lookup; a key whose compare is a
# call takes its home group outside the loop, and is not what this counts.
patches = [
    ("""        auto const* groups = m_buckets.data();
        while (true) {
            prefetch_index(groups, group_idx);""",
     """        auto const* groups = m_buckets.data();
        while (true) {
            ++udm_probe_groups;
            prefetch_index(groups, group_idx);"""),
    ("""        auto const home_idx = group_idx_from_hash(mh);
        if constexpr (!detail::key_compare_is_call_v<Key>) {
            return probe_from(key, word, counter, home_idx, 0);""",
     """        auto const home_idx = group_idx_from_hash(mh);
        ++udm_probe_lookups;
        if constexpr (!detail::key_compare_is_call_v<Key>) {
            return probe_from(key, word, counter, home_idx, 0);"""),
]
for old, new in patches:
    if s.count(old) != 1:
        sys.exit("probe_from()/probe() no longer look the way this patch expects; update scripts/ab/probe_length.sh")
    s = s.replace(old, new, 1)
s = s.replace("namespace ankerl::unordered_dense {",
              "extern unsigned long long udm_probe_groups;\nextern unsigned long long udm_probe_lookups;\n"
              "namespace ankerl::unordered_dense {", 1)
open(dst, "w").write(s)
PY

"$cxx" -O2 -DNDEBUG -std=c++17 -I"$build" -I"$root/include/ankerl" \
    -DUDM_INSTRUMENTED_HEADER='"instrumented.h"' "$root/scripts/ab/probe_length.cpp" -o "$build/probe_length"
exec "$build/probe_length" "$@"
