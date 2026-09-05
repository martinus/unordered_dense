# CLAUDE.md

Guidance for working on `unordered_dense` — a single-header C++17 dense open-addressing hash map/set (`ankerl::unordered_dense::{map, set}`).

**The index is groups of sixteen, not robin hood.** Since 2026-09-05 the only index is
`bucket_type::group`: sixteen one-byte fingerprints per group compared with one SSE2 instruction
(eight per word with SWAR where there is no SSE2), the value indices in a second array, quadratic
probing over groups, and eight overflow counters per group that an insert increments in every full
group it passes and an erase decrements again. Nothing moves after it is placed and there are no
tombstones, so no rehash is ever needed. What an erase cannot undo is *where* an element went: one
placed while its home group was full stays there, so a long-churned table probes further than one
freshly built from the same contents -- measured at load 0.76, 1.14 groups per hit against 1.03 and
1.27 per miss against 1.05, plateauing after about a dozen turnovers rather than growing. That is
the difference from a tombstone design, not the absence of any drift at all; the claim that a
churned table is *identical* to a fresh one belongs to backward shift deletion and was wrongly
carried over here. `bucket_type::group_big` is the same with 64
bit value indices. The robin hood index it replaced — the packed distance-and-fingerprint field,
the four-bucket SSE2 probe, the vector shifts on insert and erase, the sentinel padding — is gone
from the header; everything below that describes it is history, kept because the measurements and
the reasoning behind them are still worth having. Paired against it on the score the group index
was 1.10x, with churn at a fixed size 1.45x, misses 1.5x, hits 1.33x, insert-erase 1.2x, and 5.5
bytes per slot instead of 8. The layouts measured and rejected on the way are in `martinus/ai#3`.

The entire implementation lives in `include/ankerl/unordered_dense.h`. Tests and benchmarks are in `test/` and build into a single doctest executable `udm-test`.

## Build (meson)

Meson and ninja are required (`pip install -r requirements.txt` if missing). Dependencies (doctest, fmt) are fetched automatically as meson subprojects via `subprojects/*.wrap`.

```sh
# one-time setup of a release build (required for benchmarking; also sets -DNDEBUG)
CXX="ccache clang++" meson setup --buildtype release builddir/clang_release

# compile (incremental, run after every change)
ninja -C builddir/clang_release
```

A debug build for development: `CXX="ccache clang++" meson setup builddir/clang_debug`.

Warnings are errors (`werror=true`, `warning_level=3`, plus `-Wconversion`, `-Wold-style-cast`, …), so code must compile clean.

## Benchmarking

The main performance metric is `bench_quick_overall_udm`. It runs fifteen nanobench benchmarks covering the most important primitives — iterate-while-modifying, random insert/erase, build-from-empty, sustained churn at a fixed size, and random find (50% hit rate) — each for `map<uint64_t, size_t>`, `map<std::string, size_t>` and `map<uint64_t, big_value>` (a 64 byte mapped value), then prints the geometric mean of the median elapsed times:

```sh
# benchmarks are marked doctest::skip(), so -ns (no-skip) is required
./builddir/clang_release/test/udm-test -ns -tc=bench_quick_overall_udm
```

The last line of output is the score, e.g.:

```
0.0767 bench_quick_overall_map_udm
```

**Lower is better.** This single number is what to optimize.

Benchmarking practices:

- Always benchmark a `--buildtype release` build (never debug).
- Record a baseline score on the unmodified code first, then compare after each change. Run each measurement 2–3 times; treat differences within run-to-run noise (~1–2%) as no change.
- Don't compare runs made at different times — even a desktop drifts by a few percent over minutes. `scripts/ab/run.sh` runs baseline (any git revision) and candidate (the working tree) interleaved in one process, on the benchmark's own workloads, and reports a confidence interval for the ratio; believe a change when the interval excludes 100%.
- Beware code-layout luck: any edit (even to never-executed code) can shift alignment and move individual sub-benchmarks by ±3%. Judge micro-optimizations by mechanism plus a focused microbenchmark, and confirm on the paired geomean, not on a single sub-benchmark delta.
- nanobench prints per-benchmark `err%`; rerun if it's high (> ~3%). A warning about CPU governor/turbo is normal on non-tuned machines — it just means more noise.
- Other useful benchmarks in `test/bench/` (e.g. `bench_copy`, `bench_game_of_life`, find variants) can be run the same way via `-tc=<name>`; run all with `-ns -ts=bench`. List all test cases with `-ltc`.

## Where the time goes

The measurements in `scripts/ab/README.md`, with the paired A/B harness that produced them, were
taken on the robin hood index: the u64 workloads bound by branch mispredictions, the string
workloads by the latency of a ~167 instruction lookup of which the hash is ~59, and a model of 16
cycles per misprediction plus instructions at ~3.5 per cycle predicting changes within a cycle or
two. The group index that replaced it mispredicts far less -- one branch per group rather than one
per bucket -- so treat those numbers as being about the workloads, which have not changed, rather
than about the current lookup. Everything below in this section is about the workloads and still
holds.

**String keys must not all be the same length.** Until 2026-09-02 every string key of
`bench_quick_overall_udm` was exactly 200 bytes. A hash dispatches on length, and one length makes
that dispatch perfectly predictable: wyhash costs 0.31 branch mispredictions per hash on lengths
spread over 4 to 200 bytes and 0.01 on a fixed length, so the benchmark could not see the
difference. 200 bytes also put every key on the heap, where a real workload keeps a good share of
them inside the `std::string`. The keys now run from 8 to 135 bytes, skewed towards short. Scores
from before that change are not comparable with scores after it.

**Integer keys must not be small sequential values.** `insert_erase` and `iterate` draw their
values from a range that grows to 20000, and until 2026-09-02 the value was the key. The hash of a
`uint64_t` is one multiply, and the top bits of a multiple of a small integer walk a lattice: 10000
such keys in 16384 buckets landed at most one to a bucket, with 39% of the buckets empty where a
uniform hash leaves 54%. Nothing collided, so the probe never probed and the shifts never shifted
-- 82% of erases moved nothing against 58% for the same values as strings -- and two changes to
the shift loops read as losses on `ie64` that were wins on every honest table. The value is now
scrambled through a 64 bit bijection before it becomes a key, so every checksum is as it was and
`ie64` sees the same table `iestr` does. A benchmark that rewards a hash for the one input it is
perfect on is measuring the input.

**Inserting has to grow the table somewhere.** Until 2026-09-02 no workload in the score did.
`insert_erase` draws its keys from a range that grows to 20000, so the map hovers at ~10k entries
and doubles its bucket array about a dozen times in eight million operations. Growth is not a
rounding error in general: building a map of a million entries costs 52% more than building the
same map after `reserve` for `uint64_t` keys and 31% more for strings, all of it rehashing, so a
map that grew badly would have scored the same as one that grew well. `build` covers it, and it
showed at once that this is where the map is weakest -- see `scripts/ab/README.md`.

**A mapped value of eight bytes hides what a dense map is for.** Until 2026-09-03 both maps in the
score held a `size_t`. Every cost of a flat map scales with `sizeof(value_type)`, because it writes
the whole value into a hash-scattered slot; a dense map writes eight bytes there and appends the
payload to a vector in order. Measured on `build` with the same `uint64_t` key,
`boost::unordered_flat_map` is 1.22x ahead at a 16 byte `value_type` and **2.14x behind** at 64 --
the same property that wins `buildstr` and loses `build64`. `map<Key, SomeStruct>` is at least as
common as `map<Key, size_t>`, and a change that gave that property up would have scored the same.
`workloads::big_value` is 64 bytes and trivially copyable on purpose: the variable under test is the
size of the value, and an owned allocation would confound it with the heap. Its checksums are the
ones the small-value maps produce, so one set of constants verifies all three.

**`build` measured the kernel's page fault handler as much as the map, until `tame_allocator()`.**
Found 2026-09-03 while adding the big-value map. A build from empty asks for megabytes and gives
them straight back, and glibc returns anything above its mmap threshold to the OS, so every
repetition faults in the same fresh pages again: 38% of `build64`'s cycles were kernel, at 2057
page faults per build, and 65% of `buildbig`'s. Worse than noise, because whether it is paid
depends on what ran *before* in the process -- anything that already freed a block that large
raises the threshold and moves the whole thing into the arena. That is how `buildbig` came to
report 4.31ms for a build costing 14.9ms on its own, while executing *more* cycles (19.2M against
16.6M) and *more* instructions than `build64`, which reported 6.04ms.

`workloads::tame_allocator()` raises `M_MMAP_THRESHOLD` and `M_TRIM_THRESHOLD` to 64 MB, so the
arena is kept and the pages are faulted once per process instead of once per repetition. Every
workload calls it, so the process is in the same state whatever the order. Measured on `build64`:
6.61ms to 3.78ms per build, 41141 page faults to 2289, and total cycles down to within 4% of user
cycles. Isolated-versus-in-score now agrees for all three maps -- `buildbig` was 14.90 against
4.31, and is 4.94 against 4.26 -- and all three run at the same effective clock, which is the check
that the kernel time is gone. A warm allocation before the build does *not* work, touched or not
(6.60ms and 6.11ms); only keeping the arena does.

What this removes is a real cost -- a program that builds one map in a fresh process does pay it --
but it pays it once, where a benchmark repeating the build hundreds of times paid it every time and
drowned the map in it. glibc only; elsewhere it is a no-op.

**Nothing measured a table that only churns.** Until 2026-09-03 every workload in the score
either grew or was measured on a table that had just been built. Backward shift deletion leaves the
table as it would have been had the erased element never been inserted, so a table that has churned
for a long time is as good as a fresh one. A design that frees a slot without undoing what once
probed past it cannot do that -- its probe sequences only grow, and it repairs them with a rehash.
Nothing in the score could tell the two apart, because `build` and `insert_erase` both keep growing
and a growth rehash resets the damage for free. `churn` grows once and then never again: fill to
50000, reserve the room, then erase one and insert one with two lookups between, holding the size
exactly there. Measured that way over two million operations at 200000 entries,
`boost::unordered_flat_map`'s lookups degrade to **1.31x** of their fresh cost and snap back on an
in-place rehash it pays for every third round -- its bucket count never changes, and the round that
repairs costs +7ns per operation -- while this map's stay flat within the noise. With string keys
both degrade, because what degrades there is the heap the key bodies live on rather than the table,
and this map churns **1.28x faster** than boost throughout. Against boost the sustained gap is much
smaller than the fresh-table one: 1.32x on `churn64` where `rhit64` on a fresh table is 1.71x, and
1.03x on `churnstr`, the narrowest of any string workload it does not already win. Scores from
before this workload are not comparable with scores after it.

**Lookup benchmarks must not replay.** Until 2026-09 the find workload of
`bench_quick_overall_udm` reset its search rng to the insertion rng's seed, so its sequence of hits
and misses repeated and a TAGE-style predictor learned much of it: 0.6 mispredictions per lookup
where a random sequence costs the scalar probe 1.35. That under-reported the cost of branchy
probing and rewarded the opposite, and it hid most of the SSE2 probe's gain. The workload now
decides every lookup with an rng of its own. `find_random.cpp` still replays.

## Dead ends of the group index (paired A/B, 2026-09-05)

Both came from reading how other maps do it, and both lose for the same kind of reason: they add
work to a path that runs on every lookup in order to help a case that is rare.

- **Cache-line aligning the value indices**, which boost and abseil get for free because their
  groups are aligned. A group's sixteen indices are exactly 64 bytes, and glibc hands back large
  allocations at 16 mod 64, so *every* group's indices straddle two lines -- which is why
  `prefetch_index` asks for two. Giving the index array a 64 byte aligned block type does what it
  should on lookups (`find64` and `rhit64` both 1.02) and costs 4-5% on `build64`, `churn64` and
  `churnbig`, for a geomean of 0.993. The likely mechanism is conflict misses: with the group array
  and the index array both at power-of-two offsets, a group's metadata and its indices collide in
  the same cache sets more often than they do when one of them is skewed.
- **A second fingerprint in the spare high bits of the value index**, which is emhash8's trick: the
  index word has to be loaded to reach the value, so bits spent there are free, and they reject a
  fingerprint collision before the value vector is touched. Eight bits cost nothing until a table
  wants more than 2^24 slots. Measured: geomean 0.975, and the losses are exactly on lookups
  (`find64` 0.912, `findbig` 0.919, `rhit64` 0.930, `churn64` 0.915). It puts an xor, a shift and a
  compare into the dependent chain of *every* lookup to avoid a value access on the 3% that have a
  fingerprint collision. It is free for emhash8 because that map has no group-level fingerprint and
  must consult the word anyway; here the group already filtered, so the second filter is redundant
  work in the hot spot. The general shape is worth remembering: a filter only pays where nothing
  cheaper has filtered first.

- **A 16 bit value index for small maps**, which is CPython's compact dict: it stores 1, 2, 4 or 8
  byte indices depending on capacity, where this always stores 4. A `bucket_type::group_small` with
  `value_idx_type = std::uint16_t` makes the index 3.5 bytes per slot instead of 5.5 and puts two
  groups' indices in one cache line. Measured on the thirteen scored workloads that fit under 2^16
  elements: geomean **0.986**, with only `rhit64` (1.03) and `findstr` (1.02) ahead and `churn64`,
  `churnstr` and `findbig` 3-4% behind. The reason kills the adaptive version too, not just this
  one: a map small enough to be indexed in 16 bits has an index of at most 128 KB, which is already
  inside L2, so halving something that already fits buys nothing -- and the maps whose index
  footprint actually hurts are exactly the ones that need more than 16 bits. The narrow loads also
  cost a zero-extension on every use. `group_small` was not kept.

Three on the value vector, the one part nothing had touched, all keeping it dense (2026-09-05).
For scale first: growth is 54% of a 200000 element integer build here and 59% of boost's, so the
rehash is not where this map loses; the pure insert path is, 140 instructions per insert against
boost's 77, spread over a probe, a vector append, a second walk in `place_group` (4% of cycles, the
ceiling on any one-pass insert) and the spills of holding two index arrays plus a vector live. The
vector's own reallocations are 3% of an integer build and 11% of a 64 byte value build.

- **Reserving the values to the index's capacity at every index growth**, so the two grow together
  instead of on their own cadences: `build64` 0.985, `buildstr` 0.972, `buildbig` 0.967, nothing
  elsewhere. The total bytes copied are the same either way; what changes is that a full copy of
  the values now lands immediately before a rehash that wants the cache for the index.
- **A slot back-pointer per value** (a parallel `std::vector<value_idx_type>`, +4 bytes per entry),
  so closing the hole an erase leaves repoints the moved element's slot directly instead of hashing
  its key and walking to it. Pays exactly where that hash is expensive: `churnstr` 1.045, `iestr`
  1.024. Costs everywhere the vector grows: `build64` 0.953, `buildbig` 0.960, `iebig` 0.977,
  `churnbig` 0.984; integer churn is a tie. Net loss on the score, and 19% more memory for an
  8 byte value. The 2025 note above about storing the hash instead measured the same shape.
- **Growing the values with `realloc`** instead of allocate-move-free, for trivially copyable
  values. In isolation it removes a third of the vector's growth cost (0.242 to 0.164 ms for 16
  byte pairs, 0.713 to 0.476 for 72 byte ones, doubling to 200000), which is about 1% of an integer
  build and 4% of a big value one. It needs a container that is not `std::vector`, which the
  `AllocatorOrContainer` parameter already accepts, so it is available today as an opt-in and not
  worth changing what `values()` returns for.

**The insert path is split in two by clang, and that is most of the build gap to boost.** Found
2026-09-05 by counting instructions per insert on a reserved table, net of the benchmark loop:

| compiler | this map, insert (miss) | boost | this map, `operator[]` on a present key |
|---|---|---|---|
| clang 22 | 128 instructions, 39 cycles | 64, 26.5 | 74 instructions |
| gcc 16 | 82 instructions, 26 cycles | 55, 23 | 68 instructions |

Under clang `do_try_emplace` is its own function with a six register prologue, and it calls
`do_place_element` out of line, which clang refuses to inline at cost 480 against a threshold of
250 (`vector::emplace_back` with `piecewise_construct` is 225 of that). gcc inlines the whole
insert into the caller on its own. So the gap to boost on the insert path is 39% under clang and
11% under gcc, and the score moves with it: **under gcc this map is 1.25x ahead of boost on
`build64`** where under clang it is 0.70x behind, and 1.067 ahead of boost on the geomean without
iteration where clang has it at 0.96. The full gcc score against main is 1.165, against 1.109 for
clang, with `build64` 1.56 and `buildbig` 1.84. `scripts/ab/run.sh -c g++` reproduces it.

What was tried for clang, all measured paired on the score:

- **Forcing `do_place_element` and `place_group` inline** (`always_inline`): the miss path drops
  from 128 to 100 instructions and 39 to 32 cycles, `build64` 1.070, `churn64` 1.061, `churnbig`
  1.067, `buildbig` 1.041 -- and `operator[]` on a *present* key rises from 74 to 88 instructions,
  because the merged function pays the placement code's register pressure on the path that never
  places, so `ie64` 0.967, `iestr` 0.965, `iebig` 0.972, where half the inserts are hits. Geomean
  **1.012**, every interval excluding 100%. A no-op for gcc. Forcing everything into the caller as
  well gives the same numbers, so the hit-path cost is not about the caller's loop. Not applied: a
  mixed trade that hard-codes one compiler's heuristic, and the cleaner fix is below.
- **Handing the probe's fingerprint word and home group to an out-of-line `do_place_element`**, so
  the insert derives nothing twice: 141.7 to 143.7, i.e. nothing. The 28 instructions are the call
  boundary itself -- prologue, epilogue, a `pair<iterator, bool>` returned through memory -- not the
  duplicated arithmetic.
- **`increase_size` out of line** so the hot function shrinks: no change, clang's cost is in
  `emplace_back`, not in the growth path.
- **The erase path** is already flat: `erase(key)` is 102 instructions under clang with or without
  forced inlining of `do_erase`, `erase_group_slot` and `finish_erase`.

The next thing to try, not yet done: get `do_place_element` under clang's threshold *honestly*, by
making the common-case append cheaper for its cost model than `emplace_back(piecewise_construct,
forward_as_tuple(key), forward_as_tuple(args...))` -- for a map of trivially constructible types
that is a 16 byte store dressed as 225 units of inline cost. Under gcc the same code is already
fully inlined, so this is a clang-only 7% on builds and churn waiting on codegen, not on design.

Read and found to have nothing to transfer, with the reason in each case:

- **folly F14** is the closest relative, and its `outboundOverflowCount_` is this map's overflow
  counter exactly -- saturating, decremented on erase, used to stop a miss. Arrived at
  independently. One difference favours this map: F14 keeps *one* counter per 14 slot chunk where
  this keeps eight per 16 slot group, split by fingerprint class, so a miss here stops sooner.
- **abseil**'s probe sequence is the same triangular one, `(i^2+i)/2` over a power-of-two number of
  groups; its `next()` adds `Width` per step where `next_group()` adds one group, which is the same
  progression written differently. Its newer small-object optimization holds one element without
  allocating; this map already allocates nothing until the first insert, and the score has no
  workload of tiny maps, so it was not pursued.
- **bytell** puts a chain-head bit and a 7 bit index into a 126 entry jump-distance table in one
  byte per slot, so chains are linked lists with one byte links and the first sixteen distances are
  0..15 to keep short chains inside a block. It buys the ability to *skip* groups, and a lookup here
  visits 1.03 groups fresh and 1.27 churned. There is nothing to skip.
- **Verstable** packs a 4 bit hash fragment, an in-home-bucket bit and an 11 bit quadratic
  displacement into one 16 bit word per bucket. Same conclusion, and it confirms a detail: it takes
  the fragment from the *high* bits because the bucket comes from the low ones, which is the same
  independence this map gets by taking the group from the top of the hash and the fingerprint from
  the bottom.
- **tsl::hopscotch_map** keeps a per-bucket bitmap of which of the next N buckets hold keys
  belonging here. It is positional where the counters are numeric, but it is *coarser* -- one
  bitmap per bucket against eight counters per group -- and it maintains its invariant by moving
  elements closer to home, which is the work this design exists to avoid.
- **Go's map** evacuates one bucket per operation instead of rehashing at once. That trades total
  throughput for tail latency, which is a different goal from the one the score measures, and it is
  a redesign of growth rather than a transfer. Not attempted.

Where the map stands against others on the score, same hash for all, measured the same day:
excluding the three iteration workloads, `boost::unordered_flat_map` is level (0.96) and **ahead on
lookups alone by 11%**; `emilib` is 12% behind, `emhash7` 20%, `emhash8` 27%, `emhash5` 28%. With
iteration included this map leads all of them, because only `emhash8` is dense as well and the rest
lose 3-10x there. The standing weakness is the same one this file has always named: building.

## The robin hood index this replaced, and its dead ends

Everything below describes the index that was removed on 2026-09-05: a packed
distance-and-fingerprint field per bucket, a four-bucket SSE2 probe, and vector shifts on insert
and erase. None of it can be re-run against the current header. It is kept because the measurements
are real, the reasoning applies to anything that probes a flat array, and the same questions will
be asked of the group index.

## Optimization dead ends (verified with paired A/B runs; re-test before assuming they still hold)

The `bench_quick_overall_udm` hot paths are close to machine limits. Ideas that consistently
**regressed** and were reverted:

- Force-inlining `wyhash::hash` into the map (icache/register pressure outweighs saved call overhead).
- A branchless `do_find` fast path for scalar keys (unconditional key compare + conditional-move result): the speculative value load doubles cache misses on the ~50% miss lookups.
- Explicit `__builtin_prefetch` of `m_values[bucket->m_value_idx]` in `do_find`, and computing the moved element's hash early + prefetching its home bucket in `do_erase`: out-of-order execution already hides these latencies.
- Replacing wyhash with rapidhash (v3, 2025): the wyhash implementation here is *faster* for inputs ≥ 24 bytes in both latency and throughput; rapidhash only won at ≤ 16 bytes, and that trick (two plain 8-byte reads instead of building `a`/`b` from four 4-byte reads) has been adopted.
- An AES-NI hash (a port of gxhash, compiled with `-maes`, no dispatch): 30% *slower* than this
  wyhash on 200 byte keys and 27-55% slower in the map. Its serial `aesenc` chain has worse latency,
  and latency is what the string loop pays for. Fewer uops do not help a chain.
- SIMD probe variants: an *aligned* group of four (1-4 lanes visible from home) left the "nothing
  decided, next group" branch random and won nothing; deciding hit-vs-miss with two branches
  (scalar home probe, then the vector for the rest) mispredicted *more* than one vector decision
  (0.90 vs 0.70 per lookup) although each branch is more biased; keeping the key comparison inside
  the vector loop made the compiler spill the xmm state around `bcmp` on every string hit.
- Storing the top 32 hash bits per value so an erase never re-hashes the moved element: only the
  successful half of the erases move one, so the bound is ~7% of `iestr`, and it measured 1.4%
  for 4 bytes per element and a second container to keep consistent.
- Caching the bucket data pointer in the shift loops (the compiler already hoists it).
- Two attempts at the rehash, measured against `build64` and `buildstr` (2026-09-02): skipping the
  `memset` in `clear_and_fill_buckets_from_values`, which is redundant because
  `allocate_buckets_from_shift` hands back a freshly zeroed vector (0.7% on `build64`, ~1% *worse*
  on `iestr`); and hashing eight elements ahead in the rehash loop and prefetching the bucket each
  will land in (nothing on `build64`, ~1% worse on `buildstr`).
- Scalar attempts to take the branch out of `place_and_shift_up`. The robin hood shift asks "is this
  bucket occupied" once per bucket and the answer is a coin flip (73% of inserts shift nothing, 11%
  shift one), which cost 0.61 mispredictions per insert. Settling the first two buckets with
  conditional moves and one combined test does not help: written as `a == 0 || b == 0` it is still
  two branches, written as a bitwise or it is one branch that mispredicts just as often
  (0.608 per insert, ten more instructions). What did work is the vector version now in the
  header, which settles four buckets from one mask -- the same trade the probe made, and the same
  again for the shift down on erase. A two bucket vector version was also tried and lost to it
  (49.7 against 46.3 cycles per insert): its second slot is still a branch. Both vector shifts
  first read as ~1% *losses* on `ie64`, and that was the benchmark: its integer keys hashed to a
  lattice with no chains to shift (see "Where the time goes"). With honest keys they are 1.07x.
- Four attempts at the rehash loop (2026-09-03), and the reason they all failed. The loop in
  `clear_and_fill_buckets_from_values` costs **~21-26 cycles per element at every size from L1 to
  L3** (4.4 ns at 1000 elements, 5.4 ns at 200000): memory latency is already hidden, and what
  bounds it is an in-core chain through the bucket array -- each element's loads sit behind the
  previous element's stores. Two experiments pin that down. Adding 11 cycles of artificial latency
  between the probe load and the store address adds 17 cycles per element; reading four elements'
  home buckets before storing any of them is 1.5x faster while the array is L1-resident and nothing
  at 100000, where the score's rehashes run. Nothing that shortens the *data* side of the store
  moves it at all. So for this loop: anything that puts a load result on the path to the store
  address is a large loss, and anything that only saves instructions or mispredictions is
  invisible -- which is also why the earlier prefetch-eight-ahead and memset attempts found
  nothing. What was tried, against `HEAD` on the isolated loop and paired on `build64`:
  vectorizing `next_while_less` with the probe's four-lane window (`build64` **1.41x slower**,
  the rehash 2-3x per element even in L1: fewer mispredictions, 7% more instructions, and the
  store address now waits for the window load); the four-element batch above (neutral at scored
  sizes, worse at a million); refilling from the *old* bucket array in order so new homes arrive
  nearly sorted (**2x slower** at 100000-200000, with or without prefetching the keys: sorted
  arrival makes every element probe the bucket the previous one just wrote); and replacing the
  `tzcnt`-indexed blend tables of both vector shifts with a vector prefix-or (rehash neutral,
  erase 6% slower: the table loads were never on the chain). Do not read
  `ls_bad_status2.stli_other` as a cost here: `build` shows 1.5 of them per insert and `churn`
  1.3, on bucket-window loads that cannot overlap the previous insert's stores (odds ~3e-5), and
  shifting the stack of the same binary moves the count from 1.45 to 2.85 per insert while the
  cycles stay at 98.0 +- 0.5. Placing the bucket before appending the value, to put distance
  between those stores and the next probe's loads, cost 2-7 cycles per insert in every variant.

## Testing

Any change to `include/ankerl/unordered_dense.h` must pass the unit tests:

```sh
meson test -C builddir/clang_release unit --verbose
# or directly (runs all non-skipped tests):
./builddir/clang_release/test/udm-test
```

## Mutation testing

Coverage says a line ran. `scripts/mutate/mutate.py` says something would have noticed it
misbehaving: it breaks the header, rebuilds, runs the suite and asks whether anything went red.
What nothing notices is a hole in the tests. It never touches the working tree — every build
happens in a throwaway copy of the repo.

**The tool is two files, and one of them is shared with nanobench and oans.** `mutate_core.py` is
everything that is not about any one project — the lanes, the mutants, the baseline discipline, the
verdicts, the report — and both of those repositories hold a byte-identical copy of it. `mutate.py`
beside it is this project's adapter: where the header is, that meson configures the build, that the
binary is `udm-test`, that a lane needs `FUZZ_CORPUS_BASE_DIR`, and the measured constants behind
`--dry-run`. Roughly 1900 lines shared against 100 of adapter.

Three build systems and two test runners live in the core, and only one of each is exercised here:
meson + doctest. cmake is nanobench's, and `make` + minunit is oans's — a C project whose suite is
one `minunit` binary, which is why `Backend.build_argv` takes the whole argument namespace (make
remembers nothing, so `--make-arg CC=clang` has to ride on every build line) and why a `Harness`
that takes no filter arguments does not get offered the `--test-filter` flags at all. A flag
accepted and then ignored would run the whole suite while the fingerprint claimed otherwise.

That arrangement is what makes `scripts/test_mutate.py` worth its length: it covers the code *all
three* repositories run, including the cmake and make backends and the minunit harness this project
will never execute. So a change to the core is only part of a change — make it here, run that suite,
copy the file into the other two, and record the new hash in all three:

```sh
sha256sum scripts/mutate/mutate_core.py                    # write it into mutate_core.sha256
cp scripts/mutate/mutate_core.py ../nanobench/src/scripts/mutate/   # ... and into its .sha256 too
cp scripts/mutate/mutate_core.py ../oans/scripts/mutate/           # ... and that one's
```

`lint-mutate-core.py` fails if this copy has been edited without that hash moving with it, which is
the one failure vendoring introduces: a convenient local fix here leaves the other repositories
running something nothing tests. Comparing the `.sha256` files is how "are they in sync?" gets
answered; no lint in any of the repositories can see the others.

The everyday use is putting a *specific* bug back, which is the check that decides whether a new
test earns its place. Bugs worth keeping live in `scripts/mutate/bugs/`:

```sh
scripts/mutate/mutate.py --replace OLD NEW               # one, must match exactly once
scripts/mutate/mutate.py --bugs scripts/mutate/bugs/erase-path.txt
scripts/mutate/mutate.py --reverse HEAD                  # undo a fix, keep today's tests
```

A block whose replacement is *meant* to contain what it replaced — an inserted call, an early
return in front of code that stays — needs `<<< additive` on its fence. Without the flag such a
block is refused, because the code under test does not change and `caught` or `SURVIVED` would be a
verdict about nothing. That check came from woswoar, where three of them shipped in one session
before it existed; the one legitimate case in these two repositories is a nanobench bug that accepts
a `-` sign in front of a digit check that stays.

The other mode sweeps for holes nobody thought of, changing the header one place at a time. The two
modes compose, and a change is best asked both questions at once:

```sh
scripts/mutate/mutate.py --diff                          # whatever is uncommitted
scripts/mutate/mutate.py --diff HEAD~1                   # only what that change touched
scripts/mutate/mutate.py --lines 1278-1290,1400 --dry-run
scripts/mutate/mutate.py --bugs bugs.txt --lines 1278-1290 --reuse
```

`--diff` is the everyday mode and measures from the merge base, so a branch that has not caught up
with main does not sweep what main moved on without it.

`--dry-run` sizes a run, and reports a *range* rather than one number: the
per-mutant constants describe a mutant that **compiles**, and one the
`-fsyntax-only` pre-filter throws out costs about a tenth of that. The single
figure it used to print read high by an order of magnitude wherever most mutants
are invalid - measured, the `negation` sweep below was estimated at 11 minutes
and took **51 seconds**. Which end applies is decided by the operator and the
code, not by the machine, so it is knowable before running anything.

`--operators` picks what to change. The default is all of them, so a plain run asks every question
this knows how to ask — which is what you want from `--diff`, where the cost is proportional to the
lines you touched. Over the whole header that default is ~1600 mutants and something like an hour
and a half, so a full sweep is usually worth naming one operator instead. That is the reason they
are named at all.

Swept over the whole header at 4.9.1, they are not equally worth your time. Re-measure before
relying on these: they describe one header at one commit, and the kill rates move every time a
test is added.

| operator | mutants | time | killed | by a test | survivors to triage |
|---|---|---|---|---|---|
| `tokens` | 841 | ~47 min | not re-measured | | |
| `bitwise` | 76 | 4 min | **99%** | 87% | **1** |
| `deletions` | 665 | 14 min | 94% | 30% | 37 |
| `negation` | 24 | **51 s** | 100% | **0%** | 0 |

`negation` drops a logical `!`, and its row is the one worth reading twice: **every one of
its 24 mutants is rejected by the compiler, and not one reaches a test.** That is not a weak
operator, it is a fact about this header. Nearly every `!` here sits in type-level code -
`static_assert(!is_detected_v<...>)`, `enable_if_t<!is_map_v<Q> && ...>`, `if constexpr
(!std::is_trivially_destructible_v<T>)` - where dropping it makes the program *ill-formed*
rather than merely wrong. In oans, a C project, the same operator is 14 sites with 13 caught
and one real finding (the `!` excluding DELALLOC extents from a shared-byte count, which no
test held).

It stays in the default set anyway, and the reason is the 51 seconds: the `-fsyntax-only`
pre-filter rejects all 24 at about 2 s each rather than a rebuild each, so an operator that
is useless here is also nearly free here. Expect that unevenness between projects rather
than a uniform number - the same lesson the property tests taught, from the other side.

`bitwise` mutates `^` and `|`, which the token table leaves alone (`&` is three operators sharing a
spelling — bitwise and, address-of, and the reference declarator — and only a parser can tell them
apart). Few sites and a header made of masks and fingerprints is what makes it cheap and sharp. The
mechanism worth remembering rather than the number: a *surviving* bitwise mutant usually means the
two operands are provably disjoint, which is how the one survivor reads — `dist_inc | (hash &
fingerprint_mask)` turned into `^` is the same function, exactly as the
`static_assert(fingerprint_mask < dist_inc)` right above it guarantees.

`deletions` removes whole statements. Nearly every bug in `bugs/invariants.txt` is a form of "the
code forgot to do this", and none of those is one token. It costs *less* than the token sweep, since
half of them are rejected by the `-fsyntax-only` pre-filter rather than costing a rebuild.

Reordering is the operator that is *not* here — it was written, measured and removed, so there is
nothing in the tree to go and look at. Swapping two adjacent statements killed 45% of what it
generated and left a hundred survivors, essentially all of them two statements that never touched
the same state — member-copy chains, the run of `HASH_STATICCAST`
macros, blocks of declarations. Triaging every one of them produced no test worth writing. Reaching
the orderings in `bugs/invariants.txt` needs to move a statement *out of its enclosing block*, which
adjacent-swapping cannot do, so that operator is worth building only alongside something that can.

Mutants that could not have an effect are not generated: comments, string literals and preprocessor
lines are not code, and `std::enable_if_t<..., bool> = true>` is the SFINAE idiom whose value is
never read. A mutant in a branch this configuration does not compile is dropped once the lanes
exist, and the run says which lines those were.

A mutant costs one full rebuild of the test binary — all ~90 translation units include the header,
so there is no incremental mutant build and ccache cannot help either. That is ~100 CPU-seconds of
compiling against 3 of running the suite, which is why the lanes default to one ninja job each and
a single named bug instead gets the whole machine. Budget roughly a minute for a handful of
mutants and an hour for a sweep of a whole function.

Verdicts are `caught` (a test failed — the number worth moving), `compiler` (the build refused it),
`hang`, `oom` and `survived`. Without a sanitizer, a mutant that reads one slot past a bucket comes
back `survived` however good the tests are, so re-run anything surprising with
`--meson-arg=-Db_sanitize=address,undefined` before believing it.

Each lane runs inside a cgroup with a memory cap (`systemd-run --user --scope`), because a mutated
growth policy turns an insert into a request for more memory than the machine has. Capped, that is
one `oom` verdict; uncapped, the kernel picks the victim and it is as likely to be another lane as
the mutant that caused it. `--memory-limit` overrides the default, which is the smaller of a lane's
share of the machine and 1 GiB per ninja job; a build is never capped below what its jobs need. On
a machine with no user scope to be had — another init, no session bus, an undelegated container —
the run says so up front rather than pretending.

The lanes are the other half of the same problem: ~90 MB each in a workdir defaulting to `/tmp`,
which is a tmpfs on most current distributions, so `--lanes` buys memory as much as parallelism.
The run prints how much room it is about to take and whether that room is RAM, and refuses before
copying rather than part way through. Core dumps are disabled for the same reason — a crashing
mutant is an ordinary verdict, and each one would leave ~30 MB in a lane about to be deleted.

Lanes are configured `--unity=on`, which is 2.5x less compiling for the same work — measured, one
mutant rebuild of the whole suite: 67 CPU-seconds separately, 27 merged. The usual objection to
unity builds (touching one file recompiles its whole chunk) cannot apply to a mutant, which
recompiles every file anyway. `--meson-arg=--unity=off` turns it off.

One flaky case is enough to stop a run: the baseline refuses to score until the suite is green
twice, which is the point of it. `--exclude-filter NAME` (doctest's `-tce=`) is the honest way past
that — it names what was skipped in the fingerprint, where lowering `--baseline-runs` would not.

`scripts/test_mutate.py` covers the half of the tool that decides what a verdict *means*, and runs
in CI. It is hermetic: no compiler, no meson, no lanes, no cgroups. Since the core became shared it
also covers the other repositories' halves — the cmake and make backends, the minunit harness, the
project seam, the root-only ignore patterns — because a backend tested only where it is used is
exactly as untested as it was before. The make backend gets the most of that attention, because it
is the one with no configure step: its compilation database is read out of `make --dry-run`, and
every way that reading can be wrong is silent. A line mistaken for a compile has the pre-filter
checking the wrong thing; a real compile missed turns the filter off and costs a full rebuild per
mutant; and a `-l` left on a line that a syntax check cannot link makes every mutant come back
`compiler` under `-Werror`, which is the flattering direction.

## Fuzzing

The `fuzz` test suite replays the committed corpora in `data/fuzz/<target>` on every test run, which
only ever re-finds what has already been found. The libFuzzer targets are what go looking. They are
clang only and not built by default:

```sh
CXX=clang++ meson setup builddir/fuzz
ninja -C builddir/fuzz test/fuzz_api          # or fuzz_insert_erase, fuzz_replace_map, fuzz_string
./builddir/fuzz/test/fuzz_api -max_total_time=60 scratch-dir data/fuzz/fuzz_api
```

libFuzzer writes new inputs into the *first* corpus directory it is given, so keep `data/fuzz/...`
second and it stays read-only. Passing it alone — `./test/fuzz_api data/fuzz/fuzz_api` — quietly
fills the committed corpus with hundreds of generated files; to just replay it, run the `fuzz` test
suite (`./builddir/dev/test/udm-test -ts=fuzz`), which is what CI does. `scripts/fuzz_run.sh <target>` drives one across all cores, and
`scripts/fuzz_merge.sh <target>` merges a scratch corpus back down to the inputs that add coverage.
One body serves both modes: `FUZZ_TEST_CASE` in `test/fuzz/run.h` expands to the doctest replay case
normally, and to libFuzzer's entry point under `-DFUZZ`.

The same body also builds under AFL++, because `afl-clang-fast++` accepts `-fsanitize=fuzzer` and
links its own driver over `LLVMFuzzerTestOneInput`. Ask for the target by name — a bare `ninja`
fails, since AFL defines `FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION` itself and that is what
`fuzz/run.h` reads as "honggfuzz is driving":

```sh
CXX=afl-clang-fast++ meson setup builddir/afl
ninja -C builddir/afl test/fuzz_api
afl-fuzz -i data/fuzz/fuzz_api -o out -- ./builddir/afl/test/fuzz_api   # -i is never written to
```

`scripts/fuzz_afl.py` does all of that for you, which is worth using because the steps above are
easy to get subtly wrong:

```sh
scripts/fuzz_afl.py run              # every core, all four targets, until Ctrl-C
scripts/fuzz_afl.py run fuzz_api     # every core on one target
scripts/fuzz_afl.py sweep            # each target in turn, moving on when it goes quiet
scripts/fuzz_afl.py sweep --idle 15m # ... giving each one longer to prove it is done
scripts/fuzz_afl.py minimize         # fold the findings into data/fuzz, shrunk, with coverage
```

It builds what it needs, gives the first target's main instance the terminal so there is a status
screen to watch (the rest log to `fuzz-findings/<target>/afl-*.log`), resumes rather than restarting,
and stops everything on Ctrl-C. Committing what it produces is left to you.

`run` fuzzes every target at once, splitting the cores between them. `sweep` is for leaving alone:
it gives one target every core and moves on once that target has gone `--idle` without a new find,
which defaults to 5 minutes. The main instance keeps the terminal and its status screen, the same
as `run` — its `last new find` counter is what the moving on is based on. The deciding is done off
the queue directories rather than from `fuzzer_stats`, because afl-fuzz rewrites that file on its
own schedule and both `last_find` and `corpus_count` in it can sit unchanged for a minute at a
time -- long enough to call a target done while it is still finding things.

"Every core" means every *physical* core: the script reads `thread_siblings_list` and starts one
instance per core, pinned to it with `afl-fuzz -b`. Hyperthread siblings share a core's execution
units, so a second instance there mostly slows down the one already on it while afl-fuzz counts
both cores as busy. Which sibling represents a core is not guessable from the numbering — whether
core 0's siblings are `0,1` or `0,n/2` differs between machines — which is why it comes from the
kernel rather than from arithmetic. Where the topology cannot be read (macOS), it falls back to
`os.cpu_count()` and lets afl-fuzz place the instances itself.

Pinning with `-b` skips the scan afl-fuzz otherwise does for an unused core, so on a machine that
is already busy it shares rather than refusing to start.

Minimizing a corpus takes both tools, because neither subsumes the other: `afl-cmin` covers the same
AFL edges with far fewer files but is blind to libFuzzer's finer features, and `-merge=1` onto its
output adds back exactly the files carrying a feature it dropped. Note the `@@` — `afl-cmin` pipes
stdin by default, which this driver reports no coverage for.

```sh
afl-cmin -i data/fuzz/fuzz_api -o corpus-cmin -- ./builddir/afl/test/fuzz_api @@
cp -r corpus-cmin corpus-min && ./builddir/fuzz/test/fuzz_api -merge=1 corpus-min data/fuzz/fuzz_api
```

`.github/workflows/fuzz.yml` runs every target nightly, uploads any crash, and uploads the
coverage-increasing inputs it found. Its `minimize` dispatch input runs the two-step shrink above
instead of fuzzing. Committing what either produces stays a human decision.

## CI

`.github/workflows/main.yml` builds every leg the same way, so any of them reproduces locally:

```sh
meson setup builddir --force-fallback-for=fmt -Dcpp_std=c++17 <matrix setup_args>
meson test -C builddir --print-errorlogs
```

`--force-fallback-for=fmt` is what makes every runner build against the vendored fmt instead of
whatever the machine happens to have installed.

One leg builds `--unity=on`. It is off by default because merging translation units is bad for
development — touching one file recompiles its whole chunk — but it catches a class of problem
separate compilation hides, and an anonymous namespace stops isolating a file once its neighbours
share the chunk. Everything it caught the first time had been there and invisible: `test/app/print.h`
had no include guard, and four `test/bench/*.cpp` each defined a `bench()` that only became
ambiguous when two landed in the same chunk.

Linters (`scripts/lint/lint-*.py`, all of them via `scripts/lint/all.py`) run in the `lint` job.
Two of them pin their tool, because both tools gain checks or change their output between
releases: `clang-tidy-18` and `clang-format` 21 (`pip install clang-format==21.1.8`).
`lint-clang-format.py` *skips* rather than fails when it cannot find version 21, so a local run
with a different clang-format says so instead of reporting the tree as broken.

## Notes for sandboxed / offline environments

If meson cannot download the wrap subprojects (e.g. GitHub release tarballs blocked), fetch the doctest and fmt sources manually into `subprojects/doctest-2.5.3/` and `subprojects/fmt-12.0.0/` (matching the `directory` field of the `.wrap` files, which is what these version numbers have to keep agreeing with) with a minimal `meson.build` in each that declares `doctest_dep` (header-only, include dir `doctest/`) and `fmt_dep` (include dir `include/`, sources `src/format.cc`, `src/os.cc`) and calls `meson.override_dependency()`. Meson skips the download when the subproject directory already exists. These directories are gitignored — do not commit them.
