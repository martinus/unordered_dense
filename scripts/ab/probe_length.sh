#!/bin/bash
# Groups visited per lookup, fresh and after churn: the drift a table takes on because nothing moves
# after it is placed, and what move_home() takes back.
#
#   scripts/ab/probe_length.sh [load] [turnovers] [writing hits per round] [groups, default 4096]
#
# `turnovers` is one number or ascending checkpoints, "0.1,0.5,1,10", all on one table as it keeps
# churning; fractions are fine. Each checkpoint prints the table at that instant and, with more
# than one checkpoint, the mean over the interval since the previous one, sampled every 0.05
# turnovers: the number to read when something rebuilds the index periodically and an instant lands
# anywhere in its sawtooth ("10,20" reads the saturated mean over turnovers 10-20).
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
header=${AB_HEADER:-$root/include/ankerl/unordered_dense.h}
# PROBE_PATCHES="a.patch+b.patch": scripts/ab/ patches applied to a copy first, with PROBE_CXXFLAGS
# for their switches. The same-size rebuild (#363): rebuild_same_size.patch with
# PROBE_CXXFLAGS=-DUDM_DRIFT_PCT=25; the step-1 pull-back (#364): block96.patch+step1_pullback.patch.
# Their rebuilds and pull-backs are counted.
if [ -n "${PROBE_PATCHES:-}" ]; then
    mkdir -p "$build/patched/ankerl"
    cp "$header" "$build/patched/ankerl/unordered_dense.h"
    IFS=+ read -ra plist <<<"$PROBE_PATCHES"
    for p in "${plist[@]}"; do
        patch -s -d "$build/patched" -p2 <"$root/scripts/ab/$p" ||
            { echo "$p no longer applies to the header; the measurement it belongs to is dated" >&2; exit 1; }
    done
    header=$build/patched/ankerl/unordered_dense.h
fi
python3 - "$header" "$build/instrumented.h" <<'PY'
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
# the rebuilds and pull-backs of the patches that have them; a patch named but not hooked is an error,
# or "0 rebuilds" would read the same as "the counter is not there"
hooks = [
    ("rebuild_same_size.patch", """                clear_buckets();
                fill_buckets_from_values();""", "                ++udm_rebuilds;\n"),
    ("step1_pullback.patch", """            uncount(groups, mask, found_in, fp & 7U, next_idx);""", "            ++udm_pullbacks;\n"),
]
for patch, old, add in hooks:
    if patch not in __import__("os").environ.get("PROBE_PATCHES", ""):
        continue
    if s.count(old) != 1:
        sys.exit(f"{patch} no longer looks the way scripts/ab/probe_length.sh counts it; update the hook")
    s = s.replace(old, add + old, 1)
s = s.replace("namespace ankerl::unordered_dense {",
              "extern unsigned long long udm_probe_groups;\nextern unsigned long long udm_probe_lookups;\n"
              "extern unsigned long long udm_rebuilds;\nextern unsigned long long udm_pullbacks;\n"
              "namespace ankerl::unordered_dense {", 1)
open(dst, "w").write(s)
PY

# shellcheck disable=SC2086 # a list of flags
"$cxx" -O2 -DNDEBUG ${PROBE_CXXFLAGS:-} -std=c++17 -I"$build" -I"$root/include/ankerl" \
    -DUDM_INSTRUMENTED_HEADER='"instrumented.h"' "$root/scripts/ab/probe_length.cpp" -o "$build/probe_length"
exec "$build/probe_length" "$@"
