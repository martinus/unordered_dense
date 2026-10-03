#!/bin/bash
# Groups visited per lookup, fresh and after churn: the drift a table takes on because nothing moves
# after it is placed, and what move_home() takes back.
#
#   scripts/ab/probe_length.sh [load] [turnovers] [writing hits per round] [groups, default 4096]
#
# `turnovers` is one number or ascending checkpoints, "0.1,0.5,1,10", all on one table as it keeps
# churning; fractions are fine. Each checkpoint prints the table at that instant and the mean over
# the interval since the previous one, sampled every 0.05 turnovers, which is the number to read
# when something rebuilds the index periodically and an instant lands anywhere in its sawtooth.
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
# PROBE_REBUILD_PCT=<n> (#363): the same-size rebuild of scripts/ab/rebuild_same_size.patch, at that
# trigger; the rebuilds are counted
if [ -n "${PROBE_REBUILD_PCT:-}" ]; then
    mkdir -p "$build/rebuild/ankerl"
    cp "$header" "$build/rebuild/ankerl/unordered_dense.h"
    patch -s -d "$build/rebuild" -p2 <"$root/scripts/ab/rebuild_same_size.patch"
    header=$build/rebuild/ankerl/unordered_dense.h
    PROBE_CXXFLAGS="${PROBE_CXXFLAGS:-} -DUDM_DRIFT_PCT=$PROBE_REBUILD_PCT"
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
# PROBE_PULLBACK=1 (#364): an erase that frees a lane in group g pulls an entry of group g+1 whose
# home is g into it. The header would know such an entry by a step-1 bit per lane; this finds it by
# hashing, which costs time and not probe length, and probe length is all this counts.
if __import__("os").environ.get("PROBE_PULLBACK") == "1":
    old = """        groups[found_in].m_fingerprints[lane] = 0;
        uncount(groups, mask, home_idx, counter, found_in);
"""
    new = old + """        {
            auto const next = static_cast<value_idx_type>((found_in + 1U) & mask);
            auto& from = groups[next];
            for (std::uint8_t l = 0; l < from.m_fingerprints.size(); ++l) {
                if (from.m_fingerprints[l] == 0) {
                    continue;
                }
                auto const cmh = mixed_hash(get_key(m_values[from.m_index[l]]));
                if (group_idx_from_hash(cmh) != found_in) {
                    continue;
                }
                groups[found_in].m_fingerprints[lane] = from.m_fingerprints[l];
                groups[found_in].m_index[lane] = from.m_index[l];
                from.m_fingerprints[l] = 0;
                uncount(groups, mask, found_in, fingerprint_word(cmh) & 7U, next);
                ++udm_pullbacks;
                break;
            }
        }
"""
    if s.count(old) != 1:
        sys.exit("erase_group_slot() no longer looks the way PROBE_PULLBACK expects; update scripts/ab/probe_length.sh")
    s = s.replace(old, new, 1)
# optional: count the same-size rebuilds of a churned table (#363), where a header has them
s = s.replace("""                clear_buckets();
                fill_buckets_from_values();""", """                ++udm_rebuilds;
                clear_buckets();
                fill_buckets_from_values();""", 1)
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
