# Session Handoff — 2026-09-07 15:37 — the "index structures" blog post

## Resume prompt
Paste this into a fresh session:
> Read `handoff/2026-09-07_1537-index-structures-post.md` in the unordered_dense worktree
> `/home/martinus/gra/unordered_dense/dawncorn`. The plan for a long reference blog post comparing
> the index structures of fast C++ hash maps is approved and every source dependency is set up and
> verified to compile. Continue with step B of the work plan (the harness), in the order given.

## Goal
A blog post at `/home/martinus/gra/martinus.github.io/rubybolt/_posts/2026/2026-09-DD-hash-map-index-structures.md`
(date = the day it is published; repo convention) that is a *reference*, almost a book: for each
practical fast C++ hash map, what sits in front of the keys — metadata layout, how one lookup reads
it, how a miss stops, what an erase leaves — with the same questions asked of every design, one
hand-drawn byte-level diagram per map in the house style, a summary table, benchmarks and hardware
counters on one machine, and every idea PR #219 borrowed from these maps recorded inside the
chapter of the map it came from. Numbers serve the designs; the post is not a measurement story.

Decisions the user took (do not re-ask): all maps benchmarked in one new harness; one long post with
a hand-written TOC; Verstable and ihtab included as shorter sections; node-based maps covered at the
surface as a third family; one diagram per map; **no separate "ideas from the PR" part** — each
borrowed idea lives under "Measured in the group index" in its source map's chapter, and the group
index's own experiments are subsections of its chapter by design axis. Measurement methodology is one
sentence in the opening and one chapter at the very end, not a chapter in Part I.

Assumption stated in the post: every map gets this library's hash (compares *indexes*); boost's and
abseil's own-hash rows shown once as a control (CLAUDE.md measured that this reverses the string
ranking).

## Constraints that stay in force
- Every unordered_dense change goes through a PR (enforce_admins on); the harness goes in via PR.
- This is a git worktree; never `cd` to the original repo. Never bare `git stash`/`git stash pop`.
- Commits end with `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`.
- Blog posts: three front-matter keys only (`layout: post`, `title`, `subtitle`), `#` sections with
  narrative titles, figures embedded as `[![alt](/img/...)](/img/...)` on their own line, no
  includes/liquid/mermaid/footnotes, GFM tables, fenced ```cpp blocks with comments carrying figure
  step numbers. Model: `_posts/2026/2026-09-04-unordered-dense-four-buckets-at-a-time.md` (read it
  first; ~5,400 words, first person, concedes where others win, every ratio linked to evidence).
- Preview the blog with the podman jekyll container (memory `blog-preview-via-podman`), not bundler.

## The document — every headline (approved)

Title **What Sits in Front of the Keys** · subtitle *The index structures of fast C++ hash maps —
SwissTable, Boost, F14, emhash8, emilib, indivi, Verstable, robin hood and the group index — read,
drawn and compared*. Opening: 3 unheaded paragraphs, then `# Contents` (hand-written TOC).

Part I — What an index has to do
- `# 1. Five questions every hash map index answers` — Home / Here? / Absent? / Where next? / Gone?; inline glossary.
- `# 2. What a lookup is made of` — mispredictions vs instructions vs dependent loads (link the cost
  model); cache lines and dTLB past L3; the load-factor sawtooth (`sawtooth.svg`) → why every ratio is an octave geomean.
- `# 3. Three families` — `## Keys in the slots: flat` · `## Keys in a vector: dense` · `## Keys
  behind a pointer: node-based` (std::unordered_map, boost::unordered_node_map, absl::node_hash_map,
  F14NodeMap — surface level: what they promise and pay; why std::unordered_map cannot be fast by
  construction; modern node maps keep the fast index and put the node behind it). Figure `families.svg`.
- `# 4. Per-slot metadata or per-group metadata` — sets up the chapter order; what to watch: how the miss stops, what the erase leaves.

Part II — The designs. Uniform skeleton per chapter: **Layout** (diagram + struct quoted with
file:line) → **One lookup** (probe loop quoted, annotated) → **Insert, erase, growth** → **Good at,
pays for** → **In numbers** (instr / cycles / branch misses / L1 / dTLB per hit and miss, in cache
and out — only what illuminates) → **Measured in the group index** (the idea it contributed, what it did there, why).
- `# 5. Robin hood with an ordered word: unordered_dense 4.11.0` — `## The trick: distance above
  fingerprint, so one compare orders both` · `## One lookup: equal, less, or keep going` ·
  `## Backward shift deletion: no tombstones, ever` · `## Good at, pays for` · `## In numbers` ·
  `## What carried into the group index`. Figure `rh-bucket.svg`.
- `# 6. SwissTable: abseil's flat_hash_map` — `## Layout: one control byte per slot, sixteen at a
  time` · `## One lookup: match H2, then match empty` · `## Tombstones and the 7/8 rule` · `## Good
  at, pays for` · `## In numbers` · `## Measured in the group index` (aligned indices 0.993). Figure `swiss-group.svg`.
- `# 7. Boost's unordered_flat_map: fifteen slots and an overflow byte` — `## Layout: group15 and
  the byte at the end` · `## One lookup: match, then is_not_overflowed` · `## An erase cannot clear
  a bit` · `## Good at, pays for` · `## In numbers` · `## Measured in the group index` (miss bound;
  fingerprint-word table; own string hash 8-31% slower). Figure `boost-group15.svg`.
- `# 8. Folly F14: one counter per chunk` — `## Layout: fourteen tags, a hosted count, an outbound
  count` · `## One lookup: double hashing, not triangular` · `## A counter an erase can decrement` ·
  `## Value, Node, Vector` (F14VectorMap = closest relative of unordered_dense; measure it too) ·
  `## Good at, pays for` · `## In numbers` · `## Measured in the group index` (single counter 0.959). Figure `f14-chunk.svg`.
- `# 9. emhash8: chaining through the index, and a fingerprint for free` — `## Layout: {next, slot}
  per bucket, values packed in a vector` · `## The trick: hash bits above the mask` · `## One
  lookup` · `## Good at, pays for` · `## In numbers` · `## Measured in the group index` (second fingerprint 0.975). Figure `emhash8-index.svg`.
- `# 10. emilib: a state byte per slot, group-aligned homes` — short; 12% behind; nothing borrowed, say why. Figure `emilib-state.svg`.
- `# 11. indivi: counters an erase can undo, and distance nibbles` — `## Layout: sixteen fragments,
  eight counters, sixteen nibbles` · `## One lookup, and a miss that stops on a counter` · `## Erase
  by iterator without a hash: the nibbles` · `## flat_wmap: the same without groups` · `## Good at,
  pays for` · `## In numbers` · `## Measured in the group index` (ancestor of the counters; nibbles +
  back-pointer 1.10x strings / 0.959 score; the shared unbounded-miss hole). Figure `indivi-metagroup.svg`.
- `# 12. The group index: unordered_dense 5.0` — `## Layout: an 88 byte block` · `## The
  fingerprint word, from a table` · `## One lookup` (prefetch_index; NEON; SWAR carry argument) ·
  `## Eight counters, by fingerprint class — and the four other widths` (1/8/nibble/2-bit/exact
  table; per-step class: noise) · `## The miss bound` · `## Erase: decrement, do not tombstone`
  (second probe free for ints, ~50 ns strings; back-pointer 0.953) · `## Drift, and moving home`
  (pull-back 1.5x an erase; move_home; the 1.49x that was layout) · `## Where the indices live: one
  array or two` (split: tie; merged: 7% hits, 28% fewer dTLB at 4M; aligned 0.993; 16-bit 0.986) ·
  `## Growth: the pipelined rehash` (aliasing latency; radix: tie; not zeroing: 1.7% and UB) ·
  `## What the compiler decides` (probe inline gcc 1.149→1.244; do_place_element clang 1.012) ·
  `## The hash it is given` (AES 1.28x/0.63x; four latency tunings worthless; independent blocks) ·
  `## Good at, pays for` · `## In numbers`. Figures `group-block.svg`, `drift.svg`.
- `# 13. Two more, measured rather than read` — `## Verstable: a 16 bit word with a chain in it` ·
  `## ihtab: eight slots at half load` · `## ixhtab, and the bug a constant-size churn finds`
  (vnmakarov/ihtab#2). Figures `verstable-word.svg`, `ihtab-group.svg`.

Part III — Side by side
- `# 14. The summary table` — rows: 4.11.0, SwissTable, boost flat, F14, emhash8, emilib, indivi,
  Verstable, ihtab, group index, std::unordered_map, boost::unordered_node_map. Columns: keys live ·
  metadata per slot · compared at once · fingerprint bits, from where · empty/deleted encoding ·
  probe · a miss stops on · tombstones · moves after placement · max load · bytes per entry (8 B
  value, octave) · iteration · pointer stability · SIMD required · bounded on a hostile hash. Bold
  each design's distinguishing cell; three paragraphs reading it by column.
- `# 15. What one lookup touches` — `lookup-touches.svg`: lines and dependent loads per map, hit and miss, same scale.
- `# 16. The same workloads on every map` — `## Integer keys` (`bench-u64.svg`) · `## String keys`
  (`bench-str.svg`, own-hash control rows) · `## Memory` (`memory.svg`) · `## Counters`
  (`perf-lookup.svg`) · `## The probe loops, in assembly` (objdump inner loops, annotated) ·
  `## Three ways to be fast` (fewest instructions / fewest lines / fewest mispredictions).
- `# 17. Question by question` — who is best at each question and workload; `## Which one, then` + quiz link.

Part IV — Closing
- `# 18. What is still on the table` — huge pages 22%; ARM prefetch; stats facility; F14Vector; string erase 50 ns.
- `# 19. How the numbers were made, and how to remake them` — machine, same hash + control, octave
  geomean and the sawtooth phase (`churn64` 1.19→0.78), one map per binary under 10%, no replay,
  tame_allocator, harness env vars, two commands, expected noise.
- `# Appendix: sources and versions` — map, version/commit, file:lines quoted, upstream URL.

## Work plan (order: sources → harness → counters → figures → prose)

### A. Sources — DONE, all verified to compile on 2026-09-07
| map | path | version | notes |
|---|---|---|---|
| unordered_dense 5.0 | this worktree `include/ankerl/unordered_dense.h` | 5.0.0 | group index |
| unordered_dense 4.11.0 | `git show v4.11.0:include/ankerl/unordered_dense.h` | 4.11.0 | local tag; **already has the SSE2 four-lane probe**; scalar loop is `probe_scalar` :1936-1949. Rename into a second namespace exactly as `scripts/ab/run.sh:50-52` does (`sed` ankerl→udmbase) |
| boost flat + node | `/usr/include/boost/unordered/` | 1.90 | system |
| abseil | src `~/.cache/c++-mesonlsp/wrapsWorkspace/abseil-cpp.wrap/*/abseil-cpp-20250814.1`, built+installed to `/home/martinus/gra/abseil-install` (`include/`, `lib64/libabsl_*.a`, 92 libs) | 20250814.1 | link with `-Wl,--start-group /home/martinus/gra/abseil-install/lib64/libabsl_*.a -Wl,--end-group` (plain `-l` ordering fails; `absl_low_level_hash` does not exist in this version) |
| folly F14 | clone `/home/martinus/gra/folly` (shallow, `65749da` 2026-09-04) + stub `/home/martinus/gra/folly-config/folly/folly-config.h` | HEAD | **needs `-std=c++20`** (consteval); compile in `folly/folly/container/detail/F14Table.cpp`, `folly/folly/lang/SafeAssert.cpp`, `folly/folly/lang/ToAscii.cpp`; `-I/home/martinus/gra/folly -I/home/martinus/gra/folly-config`. F14ValueMap/VectorMap/NodeMap all run (`/tmp/f14/t.cpp`) |
| emhash8 + emilib | clone `/home/martinus/gra/emhash` (`20a28e8` 2026-09-05); `-I/home/martinus/gra/emhash/include`; `<emhash/hash_table8.hpp>` → `emhash8::HashMap`, `<emilib/emihmap1.hpp>` → `emilib::HashMap` | HEAD | header-only |
| indivi | `/home/martinus/gra/indivi_collection/calmsand` (`27ff2ce`); `-I.../src`; `<indivi/flat_umap.h>`, `<indivi/flat_wmap.h>` | HEAD | header-only; metadata in `src/indivi/detail/flat_{u,w}table.h` |
| Verstable | `/home/martinus/gra/Verstable/softwave/verstable.h` | HEAD | C macro template, compiles as C++; adapter pattern in `/tmp/vst/bench.cpp` (tag struct; `_get_or_insert` not `_insert`) |
| ihtab | `/home/martinus/gra/ihtab/sololynx/{ihtab,ixhtab}.hpp` | HEAD | adapter in `/tmp/ihtb/bench.cpp` |
| nanobench with `compare()` | `/home/martinus/gra/nanobench/calmcake/src/include/nanobench.h` | 4.6 + #189 | has `targetIntervalWidth()`; `/tmp/vst/nb.cpp` is the implementation TU |

Smoke test that links everything header-only + abseil: `/tmp/smoke/s.cpp` (all C++17). F14 smoke:
`/tmp/f14/t.cpp` (C++20). Since F14 forces C++20, build the whole harness with `-std=c++20`.

Where each design is quoted from (file:line, for the Appendix and the code blocks):
- abseil: `absl/container/internal/hashtable_control_bytes.h:184-214` (kEmpty −128, kDeleted −2,
  kSentinel −1 + static_asserts), `GroupSse2Impl` :272 `kWidth=16` :273; `raw_hash_set.h:534-540`
  H1/H2 (new seed scheme; old salt scheme in 20230125.1 :509-516), `probe_seq` :322-349,
  `find_large` :2989-3006, `NumClonedBytes` :787, growth 7/8 :1180-1190.
- boost `foa/core.hpp`: design comment :155-172, metadata word + overflow byte rationale :172-217
  ("(h%8)-th bit" :182-184, "invariant under modulo 8" :198), `group15` :220 `N=15` :222,
  `available_=0, sentinel_=1` :320-321, `match_word` remap table :340-344 (0→8, 1→9),
  `is_not_overflowed`/`mark_overflow` :269-291, `pow2_quadratic_prober` :867-884, find :1720-1755,
  `mlf=0.875f` :1260, erase load decrement :2262-2266.
- folly `folly/container/detail/F14Table.h`: chunk :593-650 (`kCapacity` :611, tags :630,
  `control_` :636 with hostedOverflowCount comment :632-635, `outboundOverflowCount_` :642
  saturating 254 :625-626), `probeDelta = 2*hp.second+1` :1801, loop :1832-1868, why-not-linear :1789-1800.
- emhash8 `include/emhash/hash_table8.hpp`: `struct Index {next, slot}` :142-145, `EMH_EQHASH` :65
  and the packed slot store :69-70, `find_filled_slot` :1404-1435, `hash_main` :1697, load 0.80 :72-76.
- emilib `include/emilib/emihmap1.hpp`: states :65-68, `slot_size`/`group_index` :97-100,
  `main_bucket -= main_bucket % simd_bytes` :145, `key_hash % 253 + EFILLED` :146/:154.
- indivi `detail/flat_utable.h`: `MetaGroup` :70-78, `match_word` :79, `get/inc/dec_overflow`
  :199-223, `get/set_distance` :225-245, `MAX_LOAD_FACTOR 0.875` :308, quad probe :63/:1067,
  `GroupStats` :926-993; `detail/flat_wtable.h`: `MetaWGroup` :52-65, remap :67-86, 0.8 :287.
- Verstable `verstable.h:459-465` masks, `vt_hashfrag` :467-471, `MAX_LOAD 0.9` :969, `_get` :1549.
- ihtab `ihtab.h:18-23` constants; the ixhtab bug `ixhtab.hpp:286-290` (`els_num` at :102).
- group index (this worktree): `basic_group` :693-698, `fingerprint_words` :714-721, `block` :1293,
  `default_max_load_factor 0.8` :1393, `probe` :1628-1654, `place_group` :1658-1678, `uncount`
  :1686-1697, `move_home` :1732-1753, `match_fingerprint` :1567-1589, `match_zero_bytes` :1530-1535,
  `prefetch_index` :1601-1607.
- 4.11.0: bucket :595-610, `dist_and_fingerprint_from_hash` :1317-1319, `probe_scalar` :1936-1949, `probe_simd` :1958-1995.

### B. Harness — NEXT: `scripts/ab/maps.cpp` + `scripts/ab/maps.sh` in this worktree
- Generalise `/tmp/vst/bench.cpp` (read it: octave points `{10000,11487,13195,15157,17411}`,
  modes `check`/`speed [base]`/`memory`, `one_point`/`print_geomean`, the `VT_TAG` adapter) to
  N maps: udm 5.0, udm 4.11.0 (renamed), boost flat, boost flat own-hash, absl flat, absl own-hash,
  F14Value, F14Vector, emhash8, emilib, indivi flat_umap, indivi flat_wmap, Verstable, ihtab,
  std::unordered_map, boost node. Maps behind `__has_include`; include paths via env
  (`ABSL_ROOT`, `FOLLY_ROOT`, `FOLLY_CONFIG`, `EMHASH_INCLUDE`, `INDIVI_INCLUDE`,
  `VERSTABLE_INCLUDE`, `IHTAB_INCLUDE`, `NANOBENCH_INCLUDE`). Adapter interface: `insert`,
  `bump`, `count`, `erase`, `sum`, `size` (as in bench.cpp).
- Keys: reuse `test/bench/workloads.h` (`key_for<Map>`, `tame_allocator`, `big_value`,
  scrambled integers, string keys 8-135 bytes skewed short). Miss keys from a disjoint pool
  (`absent_range = 1<<63`), never `key ^ 1`. Churn inserts must be **fresh** keys (`next++`),
  not recycled from a spare pool — recycling under-reports drift by half (CLAUDE.md).
  Lookup rngs live in a `state` that outlives nanobench epochs (no replay).
- `check` mode first: every adapter against udm, 400k mixed ops, then under
  `-fsanitize=address,undefined`. Watch for `insert` semantics (Verstable `_insert` replaces;
  check emhash8/emilib `try_emplace`/`emplace` and F14's).
- Workloads per chapter 16: build, hit, miss, half, iterate, churn, insert_erase; u64 and
  string; octaves at 1K, 32K, 500K; memory steady/churned. Two runs; quote only what agrees.
- Link line (works, verified): `clang++ -O3 -DNDEBUG -std=c++20 -I<worktree>/include -I<worktree>/test
  -I$NANOBENCH_INCLUDE -I$ABSL_ROOT/include -I$FOLLY_ROOT -I$FOLLY_CONFIG -I$EMHASH_INCLUDE
  -I$INDIVI_INCLUDE -I$VERSTABLE_INCLUDE -I$IHTAB_INCLUDE maps.cpp nb.o
  $FOLLY_ROOT/folly/container/detail/F14Table.cpp $FOLLY_ROOT/folly/lang/SafeAssert.cpp
  $FOLLY_ROOT/folly/lang/ToAscii.cpp -Wl,--start-group $ABSL_ROOT/lib64/libabsl_*.a -Wl,--end-group`.
  Also build with `g++` (16.2) as the ranking check.

### C. Counters and assembly — `scripts/ab/maps_one.cpp`
- One map per binary (`-DMAP=n -DWORK=hit|miss|churnhit`), pattern in `/tmp/vst/one.cpp`.
  `perf stat -e cycles,instructions,branches,branch-misses,L1-dcache-load-misses,dTLB-load-misses`
  at 50k and 1M entries, 30M lookups, rng advancing. Net of the loop via a `MAP=none` build.
  `export LC_ALL=C` before any awk over perf output (German locale breaks printf).
- `objdump -d --no-show-raw-insn` per binary; extract each probe's inner loop for chapter 16;
  default `-march` only (`-march=native` silently turns the SSE2 compares into AVX-512).

### D. Figures → `/home/martinus/gra/martinus.github.io/rubybolt/img/2026/hashmap-index/`
- Hand-drawn SVGs, copy the `<style>`/viewBox conventions from
  `img/2026/unordered-dense/layout.svg` (viewBox 760 wide, Inter 13, classes `.mono/.muted/.cell/.fp/.dist/.idx/.sent/.arrow`,
  teal `#0f766e`, fills `#ccfbf1`/`#e0f2fe`/`#fef3c7`, hatch for sentinels). One byte = one cell
  at a common scale across all diagrams: `rh-bucket`, `swiss-group`, `boost-group15` (redraw of
  existing `boost-group.svg`), `f14-chunk`, `emhash8-index`, `emilib-state`, `indivi-metagroup`,
  `group-block`, `verstable-word`, `ihtab-group`, `families`, `lookup-touches`.
- Generated charts via `scripts/ab/plot.py` (stdlib SVG, `--bars`, validated palette, dark
  mode) from harness CSVs: `sawtooth`, `drift`, `bench-u64`, `bench-str`, `memory`,
  `perf-lookup`. Keep CSVs + script so `--redraw` reproduces every byte.

### E. Prose
- Part II from quoted source; "Measured in the group index" from the commit inventory
  (`git log --reverse --format='%h %ad %s' --date=short main..HEAD` = 113 commits; bodies carry
  the numbers) and CLAUDE.md's entries; "In numbers" from C; Part III from B+C; Parts I and IV last.
- Cross-links: robin hood basics `/2016/09/15/very-fast-hashmap-in-c-part-1/`; infobyte+hashbits
  ancestry `/2016/09/21/very-fast-hashmap-in-c-part-2/`; SSE2 window, cost model, benchmark lies
  `/2026/09/04/unordered-dense-four-buckets-at-a-time/`; quiz `/which-hash-map/`.
- On priority of the distance-above-fingerprint ordering: say "as far as I know"; ancestry is the
  2016 part-2 post's "Infobits & Hashbits", `robin_hood` 2018, refined in unordered_dense.

## Verification
- `check` green for every adapter, also under ASan/UBSan; `speed` twice, ratios agree.
- One-map-per-binary ordering agrees with the paired harness; instruction counts match the loops.
- Every code block checked against the file:line above (`git show v4.11.0:...` for 4.11.0).
- Post preview in the podman container: every figure resolves, TOC anchors jump, tables render,
  readable at phone width.

## Numbers already in hand (from CLAUDE.md / this week; re-take before quoting as final)
- Verstable vs group index, octave geomean 1K/32K/500K: build 1.30/3.24/2.75, hits 1.50/1.09/0.75,
  misses 2.48/2.04/0.97, churn 1.71/1.25/0.62. Miss counters at 50k: udm 44.1 instr / 19.0 cyc /
  0.107 br-miss; Verstable 36.7 / 38.0 / 0.812; boost 45.1 / 19.1 / 0.162.
- Group index vs boost, octave: 1.502 with iteration, 1.066 without; vs 4.11.0 1.208.
  Single-size boost ratios that flip over an octave: churn64 1.19→0.78, churnbig 1.24→0.80,
  churnstr 1.09→0.87, rmiss64 1.11→0.92.
- Borrowed-and-measured (score geomean, group index): emhash8 second fingerprint 0.975; F14 single
  counter 0.959; abseil-style aligned indices 0.993; indivi nibbles+back-pointer 0.959; CPython
  16-bit index 0.986; Verstable exact in-home bit: −0.025 groups per churned miss (2-3% in cache).
- Counter axis: 1 / 8 / 16 nibble / 32 two-bit → churned miss 2.79 / 1.26 / 1.13 / 1.15 groups; score 0.959 / 1.000 / 0.986 / 0.988.
- Memory at 8 B value, octave: udm 32.6-33.9, boost 27.7-29.2, Verstable 27.1-28.6 bytes/entry.

## Scratch left in /tmp (volatile; copy anything still wanted)
`/tmp/vst/{bench.cpp,one.cpp,big.cpp,mem1m.cpp,instr.h,ceiling.cpp,nb.cpp,sweep.txt}`,
`/tmp/ihtb/bench.cpp`, `/tmp/smoke/s.cpp`, `/tmp/f14/t.cpp`, `/tmp/F14Table.h`, `/tmp/emhash` (superseded by the clone).
