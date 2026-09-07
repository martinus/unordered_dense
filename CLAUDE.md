# CLAUDE.md

Guidance for working on `unordered_dense` — a single-header C++17 dense open-addressing hash map/set (`ankerl::unordered_dense::{map, set}`).

**The index is groups of sixteen, not robin hood.** Since 2026-09-05 the only index is
`bucket_type::group`: sixteen one-byte fingerprints per group compared with one SSE2 instruction
(eight per word with SWAR where there is no SSE2), the value indices in a second array, quadratic
probing over groups, and eight overflow counters per group that an insert increments in every full
group it passes and an erase decrements again. There are no tombstones, so no rehash is ever
needed, and nothing moves after it is placed except one thing: an element placed while its home
group was full stays where it landed after the home empties again, so a long-churned table probes
further than one freshly built from the same contents -- measured at load 0.76, 1.14 groups per
hit against 1.03 and 1.27 per miss against 1.05, plateauing after about a dozen turnovers rather
than growing -- and since 2026-09-06 a hit found inside a *writing* operation (`operator[]`,
`try_emplace`, `insert`) moves itself home if there is room, which takes that drift back (see the
`move_home` entry below). That drift is the difference from a tombstone design, not the absence
of any drift at all; the claim that a churned table is *identical* to a fresh one belongs to
backward shift deletion and was wrongly carried over here. `bucket_type::group_big` is the same with 64
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

`.github/workflows/bench.yml` runs the same harness on every machine in CI that can take it: linux
and arm, gcc and clang, with and without the vector compare, plus one Windows leg with clang, nine
jobs against `origin/main` and against boost where boost is installable. It works on a shared
runner for the reason the harness exists -- baseline and candidate are interleaved round by round
in one process, so a noisy neighbour cancels out of the ratio -- and for the same reason the
absolute times from those runs mean nothing and are not summarised.
`scripts/ab/summarize.py` turns a run into the markdown table that lands in the job summary.

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
**The size sweep, and the measurement mistake it took three tries to get right** (2026-09-06,
`doc/*_vs_size.svg`, `scripts/ab/sweep.cpp`). Three charts -- find with a 50% hit rate, churn, and
insert-erase -- of one map grown through 377 sample points, twenty-four per octave (193 and twelve
until the regeneration later the same day), nothing reserved, to 1M entries.

**The mistake is the part worth keeping.** The first two versions measured main to completion, then
this map, then boost. Sequential phases: anything that drifts between them -- a clock ramp, a noisy
neighbour, page placement -- lands entirely on whichever map was running, and none of it cancels.
Two runs of *identical* work then disagreed by up to 140% above 1M entries and by tens of percent
below it, and one contaminated run put a whole octave 70% high while looking perfectly smooth. That
run is what produced the "insert-erase is anomalously slow at 256 buckets" feature; there was never
anything wrong with the map. nanobench's own documentation says this plainly, and the A/B harness
next door has done it correctly all along: `compare()` runs the alternatives interleaved round by
round, so drift cancels out of the ratio. The sweep now does the same, and reports each point's
interval alongside it. Even paired it needs **101 epochs** for intervals around 5% of the ratio;
at nanobench's default 11 they were 25% wide, which is the real reason the early numbers moved.

With that fixed, and only out to 1M. **Summarise across an octave, never at a chosen load.** The
earlier version of this section quoted ratios at "load 0.79", meaning the last sample point before
*this map* doubles -- and that is a biased subsample, because boost sizes differently (max load
0.875, and 1966079 buckets at a million entries, not a power of two) and swings 4-6x across its own
octave. Reading this map at its fullest against boost at wherever its own cycle put it produced
"this map is 2.2x ahead of boost on churn below 100000 entries", which the geometric mean over the
same octave does not support. Retracted; the honest statistic averages over both sawtooths.

Octave geomean, this/boost and this/main, above 1.00 meaning the other map is ahead, from the
2026-09-06 regeneration at twenty-four points per octave (the octave starting at each size):

| workload | 1K | 32K | 128K | 512K |
|---|---|---|---|---|
| find, all hits | 1.07 / 0.76 | 1.21 / 0.72 | 1.14 / 0.76 | 1.28 / 0.70 |
| churn | 1.13 / 0.68 | 1.25 / 0.66 | 1.46 / 0.72 | 1.90 / 0.85 |
| insert and erase | **0.97** / 0.74 | 1.22 / 0.73 | 1.31 / 0.78 | 1.56 / 0.74 |

So boost is ahead of this map on all three at every size except insert-and-erase at a thousand
entries, where this map is 3% ahead -- by 1.07-1.13x at a thousand entries and 1.28-1.90x at half
a million. This map is ahead of robin hood everywhere, by 1.2-1.5x. The same table for string
keys, this/boost with this map's hash and this/main and then this/boost with the hash boost ships,
octaves starting at 1K, 32K, 128K and 256K: find, all hits 1.06 / 0.94 / **0.77**, 1.11 / 0.93 /
0.95, 1.16 / 0.91 / 0.94, 1.18 / 0.90 / 0.96; churn 1.05 / 0.83 / 0.82, 1.19 / 0.92 / 1.08, 1.28 /
0.92 / 1.12, 1.38 / 0.92 / 1.21; insert and erase 1.04 / 0.85 / 0.78, 1.13 / 0.92 / 0.95, 1.16 /
0.92 / 0.98, 1.22 / 0.91 / 1.03. The this/main string column is where the new hash shows, since
main still has the old one. The dense
map's answer to that is the two charts boost is not on: 10.9x on iteration and a build that is faster
at every value size. What is *not* true, and was asserted here for a day, is that the counters buy a
win over boost on churn at small sizes.

The flatness claim survives, because it is about one map's own curve rather than a comparison: over
a fully sampled octave from 1K to 64K, cheapest point to dearest, this map swings 1.2-1.5x on churn
where boost swings 4.2-6.1x and 4.8.1 swings 2.1-2.4x.

**Where a year of this got to, measured against 4.8.1** (2026-09-06, `scripts/ab/run.sh -r 3234af2
-b all 12`, the revision `main` stood at on 1 January 2026: scalar robin hood, no vector probe
anywhere). Score geomean **1.467 under clang and 1.434 under gcc**, 1.60 and 1.57 without the three
iteration workloads, with `build64` 2.60, `churn64` 2.16, `rhit64` 2.16, `churnbig` 1.99 and
`rmiss64` 1.98 leading it and iteration at 1.00-1.08, which nothing this year touched.

Two results in that run matter more than the geomean, and both are in `scripts/ab/README.md` with
their evidence. The **string hash is 4-7% slower than 4.8.1's** -- `hashstr` 0.96 clang, 0.93 gcc
with the gcc interval excluding parity, and boost's row moving with the candidate, which is the
control that says it is the hash rather than the map; the suspicion is that July's wyhash work was
tuned while every benchmark string was 200 bytes long, and is not confirmed. And **the lookup gain
is mostly at high load**. One map per binary so nothing shares a translation unit, 20M unreplayed
all-hits lookups each, ns per `find()`: at load 0.50 (33000 entries in 65536 buckets, and 132000 in
262144) 4.8.1 reads 7.21 and 8.52 against main's 6.28 and 8.47 and this map's **4.94 and 7.01**; at
load 0.79 (52000 and 208064) it reads 14.26 and 16.98 against main's 8.79 and 10.50 and this map's
**5.95 and 7.62**. So 4.8.1 is level with main at the empty end and 1.6x behind at the full end, and
1.2-2.4x behind this map throughout. **A 50% hit rate is not the average of its parts and can order the maps differently from both.**
Paired, one binary, 101 epochs, ms per 200000 lookups at 33000 entries (load 0.50): all hits main
1.367, this **1.089**, 4.8.1 1.552; all misses main 0.811, this **0.649**, 4.8.1 0.650; 50% hits main
2.037, this 1.753, 4.8.1 **1.725**. This map is fastest on each pure case and loses the mix by 1.6%,
because an unpredictable outcome costs a clean probe a fresh half misprediction per lookup (0.026 to
0.545) and costs a probe that already mispredicts 0.6 times on every hit almost nothing (0.599 to
0.605). Making the harness's own hit-or-miss select branchless moves none of it, so the branch is the
map's own "did I find it". The whole curve says how narrow the window is -- at 33000 entries, us per
200000 lookups, this map against 4.8.1: 666/782 at 0% hits, 975/995 at 10%, 1314/**1275** at 25%,
1736/**1729** at 50%, 1437/2013 at 75%, 1125/1769 at 100%. Every map peaks at 50%, which is where
the outcome is least predictable, so that point is the least discriminating of the three: this map
leads by 1.57x at 100% hits and by 1.00x at 50%. At load 0.79 it wins at every hit rate by 1.3-2.1x. So the mix says nothing about which lookup is faster, and `find_vs_size`
plots the mix -- which is why `find_hits_vs_size` exists beside it. On that one **4.8.1 is the
slowest of the four maps at 164 of 193 sample points**, behind this map at every size below 440000,
2.86x behind it at 3251 entries and load 0.79, and swinging 2.05-2.35x across an octave against this
map's 1.07-1.28x. The eleven points where it leads are all above 440000 entries, where everything is
waiting on memory.

`doc/find_vs_size.svg` carries 4.8.1 as a fourth line and is the picture of that. Over one octave
4.8.1 swings **1.26-1.59x** between its cheapest and dearest point, against 1.11-1.28x for main,
1.04-1.14x for this map and 1.10-1.22x for boost; point by point 4.8.1 runs from 0.91x of this map
just after a doubling to 1.30x just before one. The year did not make the best case much faster, it
removed the worst case.

**`scripts/ab/regen.sh` rebuilds everything in `doc/`** -- baseline headers, the three tools, seven
measurements, eight SVGs and the page -- and `--redraw` does the drawing half alone in a fifth of a
second, which is what to use after touching `plot.py` or `dashboard.py`, since it reproduces every
SVG byte for byte from unchanged CSVs. `--quick` runs the whole pipeline coarsely in ten minutes,
which is how to find out that a tool no longer compiles without spending four hours. It refuses to
start against a nanobench without `targetIntervalWidth()` and says to point `NANOBENCH_INCLUDE` at a
checkout of martinus/nanobench#189: the sweep asks for a precision instead of naming a round count,
and the vendored 4.6.0 cannot do that. Nothing else in the repository depends on the branch.

**The same-hash convention was flattering boost on every string chart, and the control found it**
(2026-09-06). Handing every alternative this map's hash is the right way to compare *indexes*, and it
is what this project has always done -- but it is not what a caller gets, and on string keys the
difference reverses the answer. Boost with the hash it ships with, against boost given this wyhash,
geomean per octave (above 1.00 means its own hash is slower):

| workload | uint64_t, 1K to 512K | std::string, 1K to 256K |
|---|---|---|
| find, all hits | 0.93 to 0.99 | **1.30 to 1.22** |
| find, 50% hits | 0.98 to 0.99 | 1.31 to 1.27 |
| churn | 1.00 flat | 1.17 to 1.09 |
| insert and erase | 0.99 to 1.00 | 1.26 to 1.19 |

For an integer key boost's default is **1-7% faster** than this wyhash: `boost::hash<uint64_t>` is
close to the identity, foa mixes internally anyway, and the multiply is pure cost -- and once the
table leaves cache it buys nothing, which is why the column walks back to 1.00. For a string it is
**8-31% slower**, its default string hash not being wyhash.

**That flips the string ranking.** This map against boost, geomean per octave, above 1.00 meaning
boost is ahead:

| string workload | vs boost + this wyhash | vs boost's own hash |
|---|---|---|
| find, all hits | 1.07 to 1.17 | **0.82 to 0.96** |
| insert and erase | 1.10 to 1.12 | **0.87 to 0.97** |
| churn | 1.09 to 1.33 | 0.93 at 1K, boost ahead above |

So "boost is ahead on string lookups", said repeatedly in this file and its README, is a statement
about boost *with this project's hash*. Out of the box, `boost::unordered_flat_map<std::string, V>`
is 4-18% slower than this map on string lookups. The integer picture is unchanged -- boost leads
there with either hash. Both are worth having, which is why both are on the charts, but only one of
them is what a reader gets by typing the type name.

**A fifth series, and it is a control** (2026-09-06): the same `boost::unordered_flat_map` with the
hash it ships with, beside the one given this map's hash. Every other alternative on the charts is
handed this map's hash so that what differs between them is the index; that one says what the hash
choice is worth on its own, and is what a caller actually gets by passing no third template argument.
It is drawn dashed in boost's own colour, and hatched on the bar charts, because no fifth hue clears
the colourblind floor against the other four -- the best candidate is 2.7 apart from the blue under
deuteranopia against a floor of 8 -- and because colour for the map and style for the hash is the
truer encoding anyway. `robin hood (main)` is relabelled **4.11.0**, which is what it is.

The two value-size charts are **bars**, the size-axis ones lines: six value sizes are categories, and
a line between 32 and 48 bytes interpolates something nobody measured. `--bars` in both `plot.py` and
the dashboard, grouped, anchored at zero, 2px of surface between neighbours. Build and iteration are
separate charts rather than panels of one, since they differ by two orders of magnitude.

**Every workload is measured for both key types** (2026-09-06): `sweep.cpp`, `valuesize.cpp` and
`memory.cpp` all take a key argument, and the string half uses the scored benchmark's own
`key_for` -- 8 to 135 bytes skewed towards short, because one fixed length makes the hash's length
dispatch perfectly predictable. Two things changed in the tools to make that possible and are
improvements in their own right: the miss keys now come from a disjoint pool built up front rather
than from `key ^ 1`, so a miss is absent by construction and is not a neighbour of a key that is
present; and churn takes its insert from a spare pool and gives back what it erased, so no key is
constructed inside the timed region -- which for a string would have measured the allocator.
`memory.cpp` counts by replacing global `operator new` rather than through a container allocator,
because a string's body is allocated by `std::allocator<char>` inside the string and a container
allocator never sees it; for integer keys the two methods agree to the byte.

**The charts are also a page you can interrogate** (2026-09-06). `scripts/ab/dashboard.py` writes
`doc/charts.html` from the CSVs in `doc/`: every chart, a legend that shows and hides a map across
all of them at once, a y axis that rescales to whatever is left, a crosshair that reads exact values
and each map's multiple of the fastest, the hidden set carried in the URL so a view is a link, and
light and dark from the same validated palette `plot.py` uses. Self-contained, stdlib only, no
network. It does not replace the SVGs, because GitHub strips scripts out of an SVG and will not run
any of it -- the SVGs are what the READMEs embed and the page is what you open when a line looks
wrong.

**Why a dense erase is not slow, and where it is** (2026-09-06, asked as "don't we have to hash more
because of the moved element?"). We do: `finish_erase` moves `m_values.back()` into the hole, then
re-hashes that element's key and runs a second probe, `slot_of_value`, to find the slot pointing at
it. Two hashes and two probes per successful erase. Measured at a million entries, erase of a random
key plus an insert against a sequence with the same two cold probes but no move at all (a find, an
insert, and an erase of the element already last):

| | with the move | without |
|---|---|---|
| `uint64_t` keys | 57.0 ns | 56.2 ns |
| `std::string` keys | 410.4 ns | 377.7 ns |

**For an integer key it is free**, and three things make it so: the moved element is always the back
of the value vector, which in a churn loop is the same handful of cache lines and stays hot; the
first thing `do_erase` does is prefetch `m_values.back()`, so that load runs under the counter walk
that follows; and an integer hash is one multiply, so the group access it produces issues early
enough to overlap the erase's own probe and the insert's placement rather than queue behind them.
Netting out the extra operation the no-move sequence needs, the second hash and probe come to about
7 ns at a million entries.

**For a string key it costs about 50 ns**, because the hash is wyhash over 8 to 135 bytes behind a
heap pointer -- a dependent load, then a long chain -- and none of that overlaps. That is the same
thing the rejected back-pointer measured from the other side: `churnstr` 1.045 and `iestr` 1.024 for
it, against `build64` 0.953. Worth remembering that the rejection was scored on a suite whose string
tables are 200000 entries and cache-resident; the charts now cover the regime where the cost is
largest, so it is a candidate for the same re-test the merged block just had.

**Two optimizations the charts point at, one measured and one not yet** (2026-09-06, from asking
what the size sweeps imply rather than what they say).

**Huge pages are worth 22% of a large lookup and nothing asks for them.** The charts' worst regime
for this map is the one past L3, and counting there says why: at 800000 entries and all hits, this
map takes **1.48 dTLB misses and 7.03 L1 misses per lookup against boost's 0.89 and 5.15**, while
executing only 14% more instructions (95.3 against 83.2) for 33% more cycles. It is memory-side, and
a third of it is address translation -- this map touches three regions per lookup (group metadata,
value index, values) where a flat map touches one. `/sys/kernel/mm/transparent_hugepage/enabled` is
`madvise` on this machine, which is a common default, and neither the map nor the benchmark ever
madvises, so all of it runs on 4 KB pages. Handing both maps an allocator that `mmap`s 2 MB-aligned
and `madvise(MADV_HUGEPAGE)`s, ns per hit: at 800000 entries **this map 17.10 to 13.32 and boost 9.75
to 7.58**, both about 22%; at 200000, 7.18 to 7.10 and 5.47 to 5.29, which is nothing. So it is free
speed in exactly the regime the score cannot see -- 200000 entries is the largest thing the score
builds -- and it does not change the ranking, since it helps both equally. Worth doing as an opt-in
allocator and worth documenting; not worth pretending it closes the gap to boost.

**Merging the group metadata with its own value indices: measured, and kept.** The claim above that
it had never been tried was wrong -- the split's own comment in the header recorded trying it and
finding the split "10% faster on a build for the same lookups". I had read `CLAUDE.md` and not the
header. So this is a re-test, and it is the same shape as the back-pointer case: the earlier verdict
came from a regime that does not cover where the cost now is.

One 88 byte block per group -- 16 fingerprints, 8 counters, 16 value indices, `struct block : Group`
so every existing use of the metadata reads unchanged -- and no padding, so it is the same bytes the
two arrays took, in one allocation rather than two. Memory is unchanged to the byte: 27.0 per entry
steady and 32.5 at the growth peak, exactly as before.

Against `HEAD`, two independent paired runs under clang and one under gcc: score **1.018, 1.022 and
1.015**, find **1.045, 1.053 and 1.044** as a group, `rhit64` 7.1-7.5%, `findbig` 6.2%, and builds,
churn and insert-erase neutral (`build64` 100.0% and 101.2% under clang, build 0.999 under gcc,
intervals straddling parity). So the build advantage the split was kept for is gone, most likely
taken by the rehash's store-to-load fix, which changed where a build spends its time.

The mechanism is in the counters rather than in the score, which is what makes it believable -- one
map per binary, all-hits lookups at 200000, 800000 and 4M entries, split against merged: **7% fewer
instructions** (66.9 to 62.0 per lookup at 200000), because the index is at a fixed offset from the
group rather than a second address to compute; **12-14% fewer L1 misses**; and **28% fewer dTLB
misses at 4M** (5.30 to 3.79), because a lookup touches two regions rather than three. Cycles per
lookup 46.6 to 42.5, 96.1 to 88.6, 470.8 to 457.5.

`hashstr` is the control worth keeping in mind here: it never touches the map, and it read 0.923 in
one run and 1.126 in the other. That is the size of pure code-layout luck in this binary, and it is
why the counters rather than the score are what this rests on.

Three `pmr` tests failed on it, all of them counting allocations: a table is now the values plus one
index array rather than two. They asserted 5, 3, 3, 6 and 2 as literals; they now derive from
`index_t::array_count`, which is what that constant is for and what `lazy_bucket_allocation.cpp`
already did.

**The four charts worth keeping, and the two axes that had none** (2026-09-06). Asked which four
graphs decide a map, the answer needed two new tools, because two of the four axes were unmeasured:
`scripts/ab/valuesize.cpp` (build and iteration against `sizeof(mapped_type)`) and
`scripts/ab/memory.cpp` (bytes per entry, steady and peak, from a counting allocator). The set is
find-that-hits against size, churn against size, build-and-iterate against value size, and memory
against size -- see `scripts/ab/README.md` for why each and why not the alternatives.

What the two new ones say. **Value size**: boost against this map is 1.37x on a build at an 8 byte
value and 2.10x at 64, and 10.5x on iteration at 8 bytes falling to 2.3x at 64. That updates the
older note above, which had boost *ahead* at 16 bytes and behind at 64 -- the crossover moved off the
chart entirely when the rehash store-to-load fix made building 1.80x faster, so this map now leads at
every value size measured. The axis stops at 64 bytes because 200000 entries of a 64 byte value is
14 MB and still in L3 where 128 is 27 MB and is not, and past that every line bends upward together.
**Memory**: the axis that matters is the mapped-value size, not the table size. Per entry, memory
barely moves with the entry count -- the size chart is sixteen identical octaves -- but the ratio to
a flat map is a function of the value: at a million entries this map against boost is **1.19x** at an
8 byte value, 1.65x at 64 and 1.81x at 256 steady, and 1.48x, 1.81x and 1.86x at the growth peak,
because a flat map pays for its empty slots at the full width of the value where a dense one pays
four bytes of index. Charting the 8 byte case alone, which the first version did, shows the dense
design at its least impressive. Absolute, at a power of two: 27 bytes per entry against 32 for main,
4.8.1 and boost, peak 32.5 against boost's 48 and main's 40. 4.8.1's peak is 32, *lower* than main's
40, which is the price of `8d0e17e` -- building the new bucket array before releasing the old one is
what makes growth exception-safe, and it costs a taller transient.

**The sweep replayed its key sequence, which flattered the branchiest probe by 2.7x** (2026-09-06,
found because the 4.8.1 line came out ahead of this map and that was not believable). Each workload
seeded its `Rng` inside the timed function, so all 400 epochs looked up the same 20000 keys in the
same order and made the same hit-or-miss decisions, which a TAGE-style predictor learns. One binary,
replayed against carried-on, per 20000 all-hits lookups at 33000 entries: main 118.6 against 120.8
us, this map 104.6 against 96.1, **4.8.1 79.8 against 154.1**. Nearly all of it lands on the scalar
robin hood probe because it is the only one with branches to mispredict -- 0.575 per lookup against
main's 0.021 and this map's 0.024, from `perf` on one-map binaries. It is the mistake the "Lookup
benchmarks must not replay" entry above records fixing once already, reintroduced in a different
tool; the scored workloads are safe from it only because `find_all` does 10M lookups per epoch where
the sweep does 20000. Every chart in `doc/` predating the fix was wrong, and the sweep now keeps its
rngs in a `state` that outlives the epochs. The rule this leaves: a benchmark whose per-epoch batch
is small enough to memorise must advance its own randomness, and the check that catches it is
one-map-per-binary with `perf` -- the fixed sweep agrees with that to 3-8% and orders the maps
identically, the replayed one did not.

**Two more things the sweep got wrong, found on 2026-09-06 while adding a confidence band to the
absolute chart.** Neither is about the map.

*The first sample point of a process reads high*, because cold caches, a cold allocator and a
ramping clock are all paid by whoever goes first -- and pairing cannot cancel it, since what is cold
is the *point* rather than one alternative. At 16 entries boost came out 19% above its own value at
17, on intervals 1.5% wide. The sweep now measures the first point twice and throws the first answer
away.

*And a single paired run is still not trustworthy point by point.* One of five find runs had this
map alone reading 15-54% high at four adjacent sizes -- 4.59 ns at 64 entries against 2.86 to 3.03
in the other four -- with 0.5% intervals saying nothing was wrong and the other two maps normal
throughout. The second sighting gave the cause: adding a fourth map put boost at 4.72 ns at 128
entries where every other run says 2.4-2.6, wrong from 128 to about 8192. The sweep *grew* its maps
through the sample points, so each map's addresses depended on the interleaved history of the
others' allocations and an unlucky layout persisted until the next reallocation. It now rebuilds
every map at every point, which costs about a second across the whole sweep and makes a bad layout
spoil one point rather than a stretch. The rule stands anyway, since nothing makes one run right on
its own: read a chart for its shape, and check a surprising *point* against a second run. On a quiet machine
two runs agree to 0.78% median on the absolute median epoch, 0.49% on the fastest epoch and 0.92% on
the ratio -- and 9.3%, 6.9% and 6.0% at their worst points, which is what the confidence bands
drawn on the charts do *not* bound, since a within-run interval says nothing about what differs
between two runs.

**Retracted, because the data behind them does not reproduce**: that boost's find lead "peaks around
2M and narrows again", that main overtakes this map on churn above 8M, and every other number this
file previously carried for tables above 1M. The sweep is capped at 1M for that reason. Measuring
the regime above it needs a fresh process per size, not one incremental pass, because at multi-GB
sizes the result depends on page placement that varies from run to run.

**Two regimes the score does not cover, measured 2026-09-06 when asking what a more realistic
benchmark would be.**

*Small, short-lived maps* -- build, use and destroy, 2000 times, `map<uint64_t, size_t>`. Expected
to be a weakness, because the index is now two allocations rather than one and a lookup touches
three cache lines rather than two. It is the opposite: against main 1.90x at 8 entries, 2.55x at
32, 1.64x at 128 and 1.52x at 1024, and ahead of boost at every size except 8 (where boost is
1.97x main against this map's 1.90x). The reason is in the counters: branch mispredictions run
0.0-0.8% here against main's 0.8-3.4%, which is the robin hood shift being a coin flip and the
group probe not being one. Nothing to fix; worth a workload only to keep it that way.

*Tables far larger than cache* -- this is the real gap. The score's largest is 200000 entries,
whose index is about 1 MB and sits in L2 or L3 on any machine that runs it, and the value index is
one dependent load that a flat map does not pay. Half-hit lookups, same hash, this desktop:

| entries | main | this map | boost |
|---|---|---|---|
| 200000 | 9.8 ns | 7.1 ns | 5.9 ns |
| 8000000 | 54.4 ns | 42.8 ns | 30.4 ns |

So between those two sizes boost's lookup lead grows from **1.20x to 1.41x**, and the score only
ever sees the small end of that. Against main this map is still ahead at both sizes (1.38x and
1.27x), so it is a design cost rather than a regression -- but it is the one cost this benchmark
suite systematically understates, in the same way it once understated growth, churn and big values
before workloads were added for them.

**Both of those points come from one unpaired measurement each and should be read as indicative.**
A later attempt to turn them into a trend, by sweeping the size axis to 134M, produced a shape --
lead peaking near 2M and narrowing again -- that did not survive re-measurement and has been
retracted; see the sweep entry above for what went wrong and what the paired version says. What is
safe to say is that the lead exists at both of these sizes and that any claim about it has to name
the size it was taken at. What makes it awkward to add as a *scored* workload is the price: 8M entries is
~170 MB and about a second to build, so it would dominate the suite. So it is a tool rather than a
score entry -- `scripts/ab/sweep.cpp` walks every power of two from 16 to 8M for the working tree,
a baseline revision and boost, and `scripts/ab/plot.py` draws the CSV as an SVG with nothing but
the standard library. `doc/lookup_vs_size.svg` is the result, committed beside its CSV, and
`scripts/ab/README.md` says how to regenerate it. It measures the scored find workload's case, a
random lookup with a 50% hit rate decided by its own rng, and draws two linear panels: sizes to
64K, which is every table most programs build, and the whole range, which now runs to 134M entries
-- about 5 GB of map, and far enough past the caches that the curve flattens into the
memory-latency-bound plateau where the ratios stop moving. Three flat, close lines to
about 128K entries, then all three turn upward together and the gap to boost opens as they do --
and, because nothing is reserved and the sampling is twelve points per octave, a sawtooth all the
way along as each table's load factor climbs to the maximum and falls back at the doubling. The
amplitude of that sawtooth is itself a result: robin hood swings by about a factor of two between
an empty table and a full one, where the group index and boost barely swing, because a group is
compared whole whatever its occupancy.

**Pulling a displaced sibling home on erase, to take the churn drift back** (2026-09-06). The
idea: when an erase frees a slot in group g and any of g's eight counters is nonzero, look one
group along g's sequence for an entry whose class has a nonzero counter, hash its key to confirm
its home is g, and if so move it into the freed slot and decrement the counter. The counter is what
makes the check cheap when there is nothing to do -- one 8 byte load -- and it does work: after
200 turnovers at load 0.76, groups per hit 1.137 to 1.086 against a fresh 1.032, per miss 1.252 to
1.181 against 1.052, from 1.24M pull-backs in 10M erases. Half the drift, one step deep.

What it costs is the other half of the sentence. At load 0.76 some counter of the freed group is
nonzero on **46% of erases**, fresh or churned, because counters count everything that passed the
group and not only g's own siblings, and the counter cannot tell the two apart. Each of those
scans the next group and hashes two or three candidates' keys to find out, which is two or three
value loads on the erase path. Paired, `uint64_t` keys, a churn round (erase, two lookups, insert)
at 50000 entries **39.9 to 60.3 ns**, and at 2M entries 231 to 278; what it buys on the churned
table is hits 6.20 to 5.81 ns and misses 4.19 to 3.58 in cache, and **nothing at all** out of it
(42.2 against 42.1, 13.0 against 13.0), because a one-step displacement lands in the adjacent
block, which the spatial prefetcher already brought in. So it pays 20 ns per erase to save 0.4-0.6
ns per lookup in cache, break-even at thirty to fifty lookups per erase, and never out of cache.
Not kept. The lazy version is, and it is the entry below.

**A chart for the hash, and its own control says how much of it to believe** (2026-09-07,
`doc/hash_vs_length.svg`, `scripts/ab/hash.cpp`). The scored suite has one hash workload and it
reports one number over a mix of lengths; that is the right summary and it hides the shape, because
a hash dispatches on length and its cost is a staircase. Two panels, and the pair is the point:
**throughput** (independent keys, as many in flight as the machine has multipliers) is what a
hashing loop pays and what hash benchmarks report; **latency** (a byte of each answer fed into the
next key, so nothing overlaps) is what a *map* pays, since the hash's result is the address of the
group to probe. That distinction is what decided AES-NI -- a quarter faster on the left panel, half
again slower on the right.

Geomean per length range, 4.11.0's time over this map's, above 1.00 meaning this map is faster:

| bytes | throughput | latency | what the code does there |
|---|---|---|---|
| 1-16 | 1.09 | **1.00** | identical source: the short path was not touched |
| 17-48 | 1.21 | 1.18 | independent blocks, two or three multiplies |
| 49-96 | 1.11 | 1.10 | four to six |
| 97-144 | 0.99 | 1.17 | seven to nine |
| 145-256 | 0.99 | 0.99 | identical source: the chained lanes were not touched |

**The two identical-source rows are the control, and they disagree with each other.** The 145-256
row reads 0.99/0.99, which is what identical code should read. The 1-16 row reads 1.09 in
throughput and 1.00 in latency -- the same instructions, 9% apart, because restructuring what sits
*after* the short path's early return moved the code around it. So this chart's throughput panel
carries about 9% of layout in it and its latency panel does not, which means the honest reading of
the 17-144 range is the latency column: **10-18%, clean, with a control at 1.00 either side of it**.
The throughput gain is real too but 1.21 should be read as "up to 1.21, of which up to 9% is not the
algorithm".

Two more things the chart says that the score cannot. `boost::hash<std::string>` is **2.0-2.2x
slower than this hash in throughput and 1.1-1.7x in latency**, growing with length -- the mix number
(8-31%) is dominated by short keys. And 4.8.1's hash is *faster than this one in throughput* over
17-144 (1.06) while being **1.24x slower in latency**, which is the clearest statement of what July's
work traded and why the mix could not see it.

Capped at 256 bytes in the SVG, on a linear axis (`--xlin`, `--xmax`, both new in `plot.py`: a table
size is exponential and belongs on log2, a key length is not, and past 256 every line is straight).
The CSV keeps the full range to 1024. One length per point, so the length dispatch is perfectly
predicted here where the scored keys cost it 0.31 branch misses per hash -- `hashstr` is the number
that includes the dispatch, and this is the shape.

**The string hash restructured: independent blocks from 17 to 144 bytes** (2026-09-06, asked as
"make the hash faster, the values may change"). wyhash chains its 16 byte blocks through `seed`,
so a 48 byte key is three multiplies in a row before the finalizer and the map cannot form a group
address until the last of them resolves; and the block loop's trip count is a data-dependent branch
that mispredicts whenever lengths vary, which the scored keys do on purpose. Now every 16 byte
block up to 144 bytes is mixed on its own with its own pair of secrets and xor-folded into one
finalizer: latency is one multiply plus the finalizer for any length in the range, and the branches
are a short chain of compares on `len` that the predictor learns from the top. The short path and
the long chained lanes are unchanged, so lengths up to 16 and past 144 hash exactly as before;
`hash_golden.cpp` was regenerated for the range between, as its own comment says to do.

Measured on the scored keys (8 to 135 bytes, skewed short), one function per binary, ns per hash:

| | clang throughput | clang latency | gcc throughput | gcc latency |
|---|---|---|---|---|
| wyhash as it was | 2.52 | 8.64 | 2.18 | 8.47 |
| 4.8.1's wyhash | 2.19 | 9.14 | 2.21 | 9.16 |
| independent blocks | **2.00** | **7.69** | **2.05** | **7.81** |

Paired on the score: `hashstr` **1.13 clang and 1.12 gcc**, `rmissstr` 1.08 and 1.10, `iestr` 1.09
and 1.08, `buildstr` 1.07 and 1.06, `findstr` 1.06 and 1.06, `rhitstr` 1.04 and 1.04, `churnstr`
1.05 and 1.00, every integer workload at parity; score 1.006 and 1.039. The "string hash is 4-7%
slower than 4.8.1's" line in the section above is closed by this, in the other direction.

Measured and rejected on the way, all on the same keys:

- **Branchless middle**: always mixing three overlapping blocks for 17-48 and six for 49-96, so
  the length decides nothing but the read offsets. Slower on both compilers (2.48 against 2.35
  throughput under clang, 2.45 against 2.04 under gcc): the redundant multiplies cost more than
  the mispredictions they remove, which is the opposite of what the probe found, because a
  multiply is a real unit of work where a group compare is not.
- **One multiply on the short path** (`mix(a ^ s ^ len, b ^ seed)` and no finalizer): the fastest
  thing measured, 1.96 and 1.95 throughput, 7.41 and 7.74 latency -- and it fails an avalanche
  test outright: at 8 bytes some output bits never flip for some input bits (worst |p - 1/2| of
  0.50, mean 0.17 against 0.003 for the two multiply version), because the two reads are the same
  eight bytes and a single product of them has no second chance to mix. Dropping the finalizer in
  the block range fails the same test more gently (bits biased 0.42/0.58 at every length). Both
  multiplies stay; "the values may change" does not extend to a hash that does not avalanche.
- **AES-NI**, asked as "how about SSE for the hash": one `aesenc` per 16 byte block into an
  accumulator and two finishing rounds, compiled with `-maes`. Throughput 1.63 against 2.18 under
  clang and 1.47 against 2.07 under gcc -- a quarter faster -- and latency **12.0 against 7.85**
  and 11.4 against 7.82, half again slower, which is what the map pays: an `aesenc` is four cycles
  and the rounds are a chain. It is the 2025 gxhash finding again on the right key lengths this
  time, and it comes with a flag the header cannot assume. SSE2 itself, the one vector ISA the
  header may rely on, has nothing that beats a scalar 64x64 multiply for mixing: `pmuludq` is two
  32x32 products, and four of them plus the adds are slower than one `mulx`. A 16 byte load is no
  faster than two 8 byte ones. So: no.

**A mutating hit moves itself home, and that takes the churn drift back** (2026-09-06, the lazy
version of the entry above). The entry a hit just found is the one candidate whose home is known
without another hash -- the probe computed it -- and whether that home has room is one
`match_empty` on a group the probe just visited. So `move_home` runs on every hit inside a path
that already writes (`try_emplace`, `operator[]`, `insert`, `emplace`, `insert_or_assign`): a hit
at home costs a compare, a hit one group out with room at home costs a load, two stores, one zero
and the counter walk `erase` already does. It is not in `find()`, const or not: a non-const `find`
on a shared map is treated as read-only by callers, and this would make it a data race.

Drift, groups per lookup at 50000 entries and load 0.76 after 200 turnovers, fresh 1.032 per hit
and 1.052 per miss, base churned 1.143 and 1.265: with one `operator[]` hit per erase **1.091 and
1.162**, with four **1.054 and 1.093** -- nearly a fresh table, and it converges rather than
plateaus because every displaced entry that is touched again goes home.

**What it is worth, and the measurement that had to be done three times to find out.** The first
number, from a paired run of the two headers in one binary at 50000 entries, was misses 4.02 to
2.70 ns -- 1.49x -- and it was wrong. One map per binary, `uint64_t` keys, a table churned forty
times through with a writing hit per round, two runs each:

| 50000 entries, churned | base | move-home |
|---|---|---|
| churn round (`operator[]` hit, erase, miss, insert) | 42.7 ns | 42.5 |
| find, hit | 5.9 | 5.7 (1.04x) |
| find, miss | 5.16 | **4.64 (1.11x)** |
| branch misses, whole run | 7.59M | 6.26M |

At 2M entries every figure is within 1-2% either way, as with the pull-back: a one-step
displacement is the adjacent block, which the prefetcher already has. So: about a tenth off a miss
and a few percent off a hit on an in-cache table that has churned with writes, nothing on the churn
itself, nothing out of cache, +2% instructions per writing hit. Kept, because it is thirty lines
with no cost anywhere measured and the mechanism is the drift's cost itself -- the branch misses
say what that cost was: with a quarter of misses continuing past home the stop-or-continue branch
mispredicts, with a tenth it does not.

The score cannot see it and reads **1.000 under clang and 1.022 under gcc**, the gcc figure being
`buildbig` at 1.40 in a workload that never calls `move_home` -- gcc's inlining in the harness
translation unit moved, which is the `probe`-out-of-line story from the group index's first week
and not this change. Its workloads either grow, and a rehash resets the drift for nothing, or churn
with `find` between the erase and the insert, which moves nothing. `sweep.cpp` mode 5 is the
scored churn's shape with the hit through `operator[]`, added for this, and the size sweeps it
produced are the third measurement and the one to learn from:

**The sweep cannot resolve a question this size, and the way to know is to run the same code on
both sides.** Two builds of the same working tree, mode 1 (no lookups, so `move_home` never runs)
against the same baseline: octave 128-255 read **0.876 in one and 1.158 in the other**, and 524K-1M
read 0.997 and 1.067. Mode 5 read 0.860 at 1M in the build where mode 1 read 1.085, and the one-map
binaries tie there to 0.1% with identical counters. A same-code control (the working tree renamed
into both namespaces) reads 1.00-1.02 at every point above 128K, so it is not the harness; it is
the code layout of a translation unit holding two headers, and it moves 3% at large sizes and 30%
at L1-resident ones every time either header changes. The within-run intervals, which are what the
charts draw, say nothing about it. The rule this leaves is the one the `hashstr` control has been
saying all along: a paired two-header measurement decides a 10% question and not a 3% one, and
anything smaller is decided one map per binary, with counters.

Two tests in `move_home.cpp`: the churn with a mutating hit every cycle, checking every key and
its value, and a steered one on the identity hash that fills a group, sends one key past it,
makes room and provokes the move. Mutation sweep of `uncount` and `move_home`, 41 mutants: 14
caught by a test, 17 by the compiler, 3 hung, 7 survived -- four of them the counter decrement,
which is the same uncovered decrement the termination-bound entry above records (now shared
between erase and the move), and the other two the "already at home" early return and its
comparison, which mutated either move an entry to another lane of its own home group, which is
legal, or silently skip the repair. Nothing that changes an answer survived. One codegen detail
found by mode 1: `uncount` takes the group pointer, mask, home and counter as arguments loaded
*before* the caller's fingerprint store, because a `std::uint8_t` store may alias any of them and
a reload after it lands on the address chain of every step of the walk. The design paragraph at
the top of this file that said nothing moves after placement is now wrong by exactly this much: a
hit inside a write may move one step, to its home, and nothing else moves.

**The rehash loop pipelined, and the partitioned rehash it was measured against** (2026-09-06,
from asking what else the group structure is good for: placement is shift-free, so a rehash can
place in any order, which is what a database-style radix partition needs). First the target,
`rehash(0)` on a built map so the loop is isolated, ns per element:

| entries | index | u64 before | pipelined | partitioned | string before | pipelined | partitioned |
|---|---|---|---|---|---|---|---|
| 200K | 1.4 MB | 2.05 | **1.63** | 3.22 | 8.7 | 8.4 | 8.5 |
| 1M | 11 MB | 2.27 | **1.78** | 5.53 | 11.9 | 8.4 | 8.6 |
| 2M | 22 MB | 6.71 | **3.52** | 5.31 | 24.8 | **9.4** | 10.0 |
| 4M | 44 MB | 10.29 | 7.63 | **5.34** | 30.6 | 12.4 | 12.1 |
| 16M | 176 MB | 12.58 | 12.46 | **8.65** | | | |

*Pipelined* hashes sixteen elements ahead of the one it places and prefetches the group each will
land in, with the group pointer, mask and shift held in locals (a fingerprint store may alias any
of them through `this`, the same shape as the rehash fix above). Below cache it is 1.26x for the
pipelining alone -- hash chain and random placement no longer wait on each other -- and CLAUDE.md's
old note that prefetching ahead in the rehash was worthless was measured at 200000 entries, in
cache, where it must be. Above cache it does everything for strings, whose hash has work to hide a
miss behind (2.5x at 2M and 4M), and nothing for integers at 176 MB, because that loop is bound by
the TLB rather than by latency: **1.15 dTLB misses per placement** on 4 KB pages, and a prefetch
cannot hide a page walk. Partitioning by the top bits of the group -- one histogram pass, one
scatter of an 8 byte packed entry per element, then placement partition by partition -- cuts that
to 0.24 and halves the loop from 4M up.

**Kept the pipelined loop, dropped the partition.** Paired, three variants in one binary, a build
from empty: pipelined and partitioned are indistinguishable end to end (u64 8M 53.8 against 53.4
ns per element, strings 4M 128.5 against 128.8), both 3-18% ahead of the plain loop. Timing every
rehash inside a build says why the isolated 2x disappears: the scatter costs 3.6 ns there instead
of 1.4, because the scratch is fresh memory every time and faulting it in costs about a microsecond
a page -- the `tame_allocator()` lesson, paid by the map itself this time -- and a rehash is a
minority of a large build anyway (15 of 64 ns per element at 16M; the inserts are the rest). So
the partition is worth 0-7% of an integer build above 32 MB, for 8 bytes of scratch per element
at the growth peak, an allocation inside `rehash()` that changes what an allocator sees and what
an exception path has to undo, and a hundred lines. The pipelined loop is thirty lines, no memory,
and the score is exactly neutral on it (1.002 clang, 1.003 gcc, `buildbig` 1.04 and 1.07), which
is what a 200000-entry suite should say about a loop whose gains are above cache. Unit suite green.
What did not matter: the partition size, 16 KB to 256 KB (place phase 3.4 ns throughout, so it was
never L2 misses); the prefetch distance, 8 to 32 (16 best by a hair). What an integer rehash above
cache would actually need is huge pages, which the note above already measured at 22% on lookups
and which the map cannot ask for.

**Not zeroing the value index** (2026-09-06, from a code review that put it at ~7% of a build).
`std::vector::resize()` value-initialises, so growing the index writes 64 bytes of zeros per group
that nothing reads: a slot's index is written when an entry is placed there and read only for a
slot whose fingerprint already says it is occupied. An allocator adaptor whose no-argument
`construct()` default-initialises removes the writes, and for the group array beside it the zeroing
is load-bearing and stays, since a zero fingerprint is what empty means.

Measured properly -- two binaries, alternated, four rounds -- it is worth **1.7%** of a two million
element build, not 7%: 42.2 ms to 41.5, consistent in direction every round, and nothing at all on
the score, whose builds are 200000 elements and zero about 2 MB instead of 32. The first
measurement said 6.8% and was wrong, having compared runs made minutes apart, which is the mistake
the benchmarking section of this file exists to prevent.

Reverted for a reason that outranks the number: `assign()` copies the whole index array, so leaving
entries default-initialised means the copy constructor reads indeterminate `std::uint32_t` values.
That works everywhere and is undefined behaviour anyway, and a library header should not have it in
a copy constructor to buy 1.7% of a large build. Copying only the occupied slots would avoid it and
costs more than it saves. All 34 CI legs including valgrind passed with the adaptor in, which is
worth knowing: valgrind tracks definedness through a copy without complaining, so it would not have
caught this either.

**What `segmented_map` gives up in 5.0.0, found in review.** `IsSegmented` used to segment the
bucket array as well, through the `BucketContainer` parameter this branch removed; now it segments
only the values and the index is two plain contiguous arrays for every table. So a segmented map
keeps stable references and smooth *value* growth, and loses the promise that nothing spikes: the
index still doubles beside itself. At 5.5 bytes per slot the transient is ~16.5 bytes per slot,
which for a 100M entry `map<uint64_t, uint64_t>` is a 1.1 GB spike against 1.6 GB of values. Kept
rather than reverted, because segmenting the index means an indirection per group access in the
probe -- the one path everything else here is spent making short -- and `reserve()` removes the
doubling for a caller who cares. The README says so plainly now; it previously called the spike
"small next to the values", which is only true when the value is large.


**Fingerprints and counters in two arrays instead of one 24 byte group** (2026-09-05). The layout
sweep in `martinus/ai#3` kept the two together in every one of its eleven layouts, so the split was
the one form never measured. The case for it: sixteen fingerprints are a quarter of a cache line,
so four groups fit a line exactly and none straddles, where a 24 byte group straddles one time in
four; the case against: a miss then needs a second line for its counter. Measured paired on the
nine workloads that touch the index, with the unchanged layout as a control: every ratio within
noise (`rhit64` 1.00, `rmiss64` 1.01, `find64` 0.99, `churn64` 1.00, `ie64` 1.02, control 1.00
throughout), and on a 20M entry table whose index is 37 MB and lives in DRAM, 57.1 against 57.0 ns
per hit and 34.3 against 34.1 per miss. A tie both in cache and out of it. The straddle is free
because the second line is the adjacent one, which the spatial prefetcher brings in with the first;
the counter line is free because its address depends only on the group, so the load issues beside
the fingerprint load rather than after it. Not kept: one array is simpler than two for the same
speed, and the same reasoning says the padded 32 byte group and the 16-fingerprint-plus-8-counter
split are the same question, already answered.

**Which counter a probe consults at each step of its sequence** (2026-09-05). The design reads the
class of the fingerprint's low three bits at every group. The false continues of a churned miss
come largely from *siblings* -- entries with the same home group, which walk the same sequence, so
a displaced sibling of the same class carries the miss along its whole displacement: 17.5% of
churned misses continue at step 0 and about half of those continue again at step 1, far above the
1/8 a fresh class check would give. Two alternatives, measured with a compile-time switch:

- **Class plus distance, `(fp + d) & 7`**: exactly a no-op, as the arithmetic says it must be --
  the sibling and the miss add the same d at every step, so if they agree at step 0 they agree
  everywhere. Churned miss 1.263 groups against 1.262.
- **Three fresh hash bits per step** (bits above the fingerprint, rotated by three per group): does
  break the lockstep, and the churned miss drops from 1.262 to 1.238 groups with the step-0 rate
  unchanged by construction. But that is 2% of a miss's probe work in the tail, on a path 17.5% of
  churned misses reach, and the paired measurement on the six workloads that can see it is noise:
  `rmiss64` 1.00, `churn64` 0.99, `find64` 0.99, `churnbig` 0.98, `findbig` 1.03, `rmissstr` 0.98,
  with the refactor control at 1.00 everywhere. The saving is real and smaller than the register the
  rotating word occupies.

Neither kept. What would move a miss is the step-0 decision, and that is the counter's class count,
which the row below settles.

**The width of the overflow counter, all four divisions of a group's eight counter bytes measured
against the design's eight one-byte counters** (2026-09-05, prompted by asking whether folly's
single per-group counter would help). It is the axis the design already sits at the top of, and the
two directions off it lose for opposite reasons. Fresh and after 200 turnovers at load 0.76, the
share of misses that continue past their home group, and the score:

| counters per group | fresh miss | churned miss | misses continuing (churned) | saturated at 200 turnovers | score |
|---|---|---|---|---|---|
| 1, F14 style (class ignored) | 1.21 | 2.79 | 60% | 0 | **0.959** |
| 8 x 1 byte (the design) | 1.06 | 1.26 | 17.5% | 0 | 1.000 |
| 16 x nibble | 1.03 | 1.13 | 9.7% | 0 | 0.986 |
| 32 x 2 bit | 1.02 | 1.15 rising | rising | 36063 | 0.988 |

Folly's one counter is the worst of the four, not the best: it does not know the fingerprint class,
so *any* overflow past a group makes every later miss into it continue, and a churned table where
most groups have seen an overflow sends 60% of misses on. Its score is 0.959 and its churn 0.83.
The idea that a shared counter saturates faster and so helps is aimed at the wrong thing --
saturation was never what stops a miss, the `delta == m_group_mask` bound is, and once saturated a
counter is *worse*, since it never comes back down. That is exactly what sinks the 2 bit counter:
it filters best of all when fresh (1.3% continue) but its max of 3 is reached constantly under
churn, and a saturated counter lengthens every later miss for the life of the array.

The nibble is the interesting direction and still loses. It genuinely filters better -- half as many
fingerprints per counter, so 9.7% of churned misses continue against the design's 17.5%, at no
memory cost and with a max of 15 that nothing reached even after 200 turnovers. But a sub-byte
counter is a load-mask-compare on the read and a read-modify-write on the increment, and that is
paid on *every* lookup, while the continuation it saves was already rare (4% fresh). So `find64`
0.908, `rhit64` 0.940, `findbig` 0.923: the per-probe arithmetic costs more than the rarer group
hop saves. The general shape is the one this file keeps rediscovering -- a filter only pays where
nothing cheaper filtered first, and here the group's own fingerprint compare already did most of it,
so a finer counter is refining a decision that is nearly always already made. A byte per class is
the point where the counter is a single aligned load and still per-class.


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
- **The same back-pointer plus indivi's 4 bit distance nibbles**, so that `erase(iterator)` needs
  no hash at all: the slot comes from the back-pointer, the counter from the slot's own
  fingerprint, and home from reversing the quadratic walk by the stored distance. On the one
  pattern it exists for, find then `erase(it)` then insert on a reserved table: **1.10x faster
  with string keys** (1006 to 888 instructions per round, one wyhash and two probes gone) and
  **1.10x slower with integer keys** (352 to 363, the saved hash is 8 instructions and the
  back-pointer maintained on every insert costs more). On the score, where every erase is by key,
  it can only cost, and does: geomean 0.959, `build64` 0.831, `buildbig` 0.893, `churn64` 0.900,
  with `iestr` 1.052 the one workload ahead. Memory 31 to 38 MB per million 8 byte values. Not
  worth an opt-in either: the win needs an expensive key *and* erase by iterator, and a caller with
  both has `erase(key)` with the hash already paid by their own `find`.
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
  well gives the same numbers, so the hit-path cost is not about the caller's loop. **Applied**: by this file's own
  rule an interval that excludes 100% on the score is a change to believe, and the trade is written
  above the attribute in the header so it can be reversed knowingly.
- **Handing the probe's fingerprint word and home group to an out-of-line `do_place_element`**, so
  the insert derives nothing twice: 141.7 to 143.7, i.e. nothing. **Returning the value index in a
  register** instead of a `pair<iterator, bool>`: 141.7 to 141.7, clang already returns that pair
  in registers. The 28 instructions are the call boundary itself -- prologue, epilogue, argument
  setup -- and only merging removes them.
- **`increase_size` out of line** so the hot function shrinks: no change, clang's cost is in
  `emplace_back`, not in the growth path.
- **The erase path** is already flat: `erase(key)` is 102 instructions under clang with or without
  forced inlining of `do_erase`, `erase_group_slot` and `finish_erase`.

The next thing to try, not yet done: get `do_place_element` under clang's threshold *honestly*, by
making the common-case append cheaper for its cost model than `emplace_back(piecewise_construct,
forward_as_tuple(key), forward_as_tuple(args...))` -- for a map of trivially constructible types
that is a 16 byte store dressed as 225 units of inline cost. Under gcc the same code is already
fully inlined, so this is a clang-only 7% on builds and churn waiting on codegen, not on design.

**Read boost's `unordered_flat_map` again after it turned out to have the probe bound this map was
missing** (2026-09-06). Three things came back, in descending order of worth:

- **MSVC had no prefetch at all.** `ANKERL_UNORDERED_DENSE_PREFETCH` was `__builtin_prefetch` for
  gcc and clang and `static_cast<void>` for everything else, so the index-line prefetch that is
  measured at 3 cycles off every hit was silently absent on a whole compiler. boost spells it
  `_mm_prefetch(p, _MM_HINT_T0)` on MSVC x86-64 and `__prefetch` on MSVC ARM64. Taken. Not
  measurable here, since none of the benchmarking runs on MSVC; it is a gap closed by construction
  rather than a win demonstrated.
- **boost tunes the prefetch per architecture** and says so in a comment: "ARM architectures get a
  higher speedup when around the first half of the element slots in a group are prefetched, whereas
  for Intel just the first cache line is best." This map issues the same two or three prefetches
  everywhere. Now that `bench.yml` exists that is a measurable question rather than a guess, and
  it has not been asked.
- **boost has an opt-in statistics facility** (`BOOST_UNORDERED_ENABLE_STATS`,
  `cumulative_stats.hpp`) that keeps running mean and variance of probe lengths and comparisons per
  lookup with Welford's algorithm. Every probe-length number in this file was produced by hand
  editing a copy of the header instead. A built-in equivalent would make the measurements this
  project keeps needing repeatable, and is the one idea here that is a feature rather than a fix.

Two details looked at and deliberately not taken. boost reserves *two* metadata values, 0 for empty
and 1 for a sentinel that ends iteration, and remaps hashes 0 and 1 to 8 and 9; this map reserves
only 0 and so has one more usable fingerprint, because it iterates the value vector and needs no
sentinel. And boost's 16 byte group is 15 fingerprints plus the overflow byte, read with an
*aligned* load and masked with `& 0x7FFF`; the 24 byte group here holds 16 fingerprints and eight
counters and is read unaligned, which the layout sweep measured as free.

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

**That last sentence stopped being true on 2026-09-05.** Re-measured against
`boost::unordered_flat_map` after the rehash fix, same hash, 12 paired epochs, boost's time over
this map's:

| | clang | gcc |
|---|---|---|
| score, 15 workloads | **1.56** | **1.49** |
| without the three iteration workloads | 1.13 | 1.17 |
| iterate | 5.76 | 3.91 |
| build | 1.62 | 1.77 |
| churn at a fixed size | 1.16 | 1.21 |
| insert and erase | 0.96 | 0.99 |
| find, half hits | 0.90 | 0.88 |
| random hits and misses | 0.92 | 0.92 |

Building is now a *win* rather than the weakness -- `build64` 1.39x and 1.64x, `buildbig` 2.09x on
both compilers -- because the store-to-load chain in the rehash was what held it back, not the
design. What is left of boost's advantage is exactly one thing, fresh-table lookups, and it is
worth stating plainly: boost is **10-13% faster on a hit** (`rhit64` 0.80, `findbig` 0.87) and this
map is faster on a miss (`rmiss64` 1.12 under clang). That is the value-index indirection, one more
dependent load than a flat map needs, and it is the price of the dense value vector -- which is the
same property that pays 3.9-5.8x on iteration and 2.1x on a 64 byte build. Memory for a million
entries, re-measured 2026-09-06 with a counting allocator and again with a replaced global
`operator new`, the two agreeing exactly: **27.0 MB against boost's 32.0** steady with an 8 byte
value, and 113.5 against 205.5 peak with a 64 byte one. The figures this line used to carry -- 39.5
against 67.9, and 144.5 against 209.0 -- do not reproduce under either method, and the boost steady
one cannot be right by arithmetic: a million entries in 1966079 slots of a 16 byte `value_type` is
31.5 MB, which is what both methods return.

**A miss had no bound, and eight chosen keys made it loop forever** (found 2026-09-05, in the
review before release). The probe stopped only at a group whose counter for the key's class was
zero, on the argument that exact counters put a zero right after the furthest entry of that class.
The argument is wrong: a counter counts entries that overflowed past its group on *their* probe
sequences, not on the one being walked. Fill a group, send one key of class 1 past it, erase the
fillers -- the passer stays, so the counter stays -- and do that for every group: eight live keys,
every class-1 counter positive, and `contains()` on an absent class-1 key never returns. Any hash
the caller controls reaches it, and the default hash with attacker-chosen keys does too, since only
the top few bits and the low byte need steering. `indivi::flat_umap`, where the counters came from,
has the same hole (`find_impl` loops on `gIndex <= mGMask`, which the mask makes always true); the
same eight keys hang it. The fix is `|| delta == m_group_mask` on the miss exit: a key that exists
was placed within one cycle of its sequence, so a walk that has seen every group can stop. By
mechanism it is free -- per lookup on a 200k table, 83.6 to 82.7 instructions on a hit, 69.5 to
67.6 on a miss, cycles and mispredictions unchanged -- and the paired score read 0.99 with three
workloads at 0.95 beside a 1.16 on `hashstr`, which never touches the map, so that run's layout
moved. `test/unit/probe_termination.cpp` builds both this table and a saturated counter with the
identity hash; the corpus fuzzers, all on wyhash, could not have reached either. The bound has a
second effect worth knowing: it converts a *missing or wrong-home* erase decrement from a hang into
a silent slowdown. That fault used to be caught loudly -- the counters only grew, a miss found no
zero, the suite hung -- and now the miss stops at the end of the array and the table stays correct,
just slower. So the erase decrement is no longer covered by any correctness test (mutating it away
SURVIVES the suite), only by the A/B score. That is the deliberate trade of making the map robust
to a hostile hash: a hang is loud, degradation is quiet, and the map has to prefer the quiet one.

**Mutation triage after the bound** (2026-09-05, `invariants.txt` and `erase-path.txt` re-run,
plus a `bitwise,deletions` sweep of the index functions, lines 1380-1560). `invariants.txt` is 45
of 46 caught after a test was added for the one real gap the sweep found: copying an *emptied but
grown* table by assignment into a grown target left the copy at the source's shift, so its first
insert allocated the large array instead of the smallest (2048 buckets against 64) -- observable,
and nothing checked it, now `copying_an_emptied_table_starts_from_the_smallest_array` in
`lazy_bucket_allocation.cpp`. The two `invariants.txt` survivors are equivalent: the moved-from mask
(every find and erase checks `empty()` before it could read the mask, and a moved-from table is
empty) and the erase decrement just above. The sweep's sixteen survivors are all one of three
kinds: deletions and bitwise rewrites inside the SWAR fallback, which an SSE2 build does not compile
at all; prefetch deletions, which are semantic no-ops; and the erase decrement again. None is a test
hole. `erase-path.txt` is 5 of 6, the sixth being that same decrement.

**Indexing the value container in the rehash cost clang a memory latency per element** (2026-09-05,
from comparing the two compilers' absolute times on the branch rather than their ratios to main).
`build64` was the one large branch-specific compiler gap left: 3.36 ms under clang against 1.97 under
gcc, where main's own gcc/clang ratio on the same workload is 1.04. Splitting a 200000 element build
into inserts into a reserved table and the growth on top of them says where it lived -- clang was
**1.44x faster** on the reserved inserts (6.30 ns against 9.08) and **4.9x slower** on growth (10.43
ns per insert against 2.15). So it was never the insert path; it was `fill_buckets_from_values`.

The loop read `m_values[value_idx]`. Placing an entry stores a fingerprint, a `std::uint8_t` store
may alias any object at all, and the container's own data pointer is such an object -- so after
every placement an indexed read has to load that pointer back out of the container before it can
form the address of the next key. The random group access that follows cannot start until that
resolves, so the chain costs one memory latency per element and the placements, which are
independent, run one at a time. Walking with an iterator keeps the address in a register: **growth
10.43 ns per insert to 2.74, the whole build 16.72 to 8.96**, i.e. clang's build is now faster than
gcc's rather than 1.7x slower. Paired on the score, clang `build64` **1.80**, `buildbig` **1.61**,
geomean **1.076**, everything else within noise; gcc 1.03 and 1.03, geomean 1.006, since it
disambiguated on its own.

What did *not* work, all measured: hoisting `m_buckets.index()` out of `place_group` (the same
store-to-load shape, but that pointer was already in a register), `__restrict__` on the group and
index pointers (says nothing about the container's pointer, which is the one being reloaded), and
every combination of forcing `place_group` and `fill_buckets_from_values` inline or out of line
(a 3x2 matrix, all within 2%). The lesson is the one the old rehash section already stated for a
different loop -- what matters is what sits on the path to an address -- and the new part is that a
byte-sized store is enough to put a container's own bookkeeping there.

**The rest of the header was then audited for the same shape, and it is the only instance.** Every
site that reads a container by index after a store: `replace()` has the identical loop and is
*correct* as written, because that loop moves elements and pops the back, so the pointer and the
size genuinely change and the reload is required; `erase_if` recomputes `begin()` around an
`erase()` that moves elements, likewise; `probe` reads `m_values` to compare keys but stores
nothing, so nothing serialises inside one probe; and `place_group` still asks for
`m_buckets.index()` after writing a fingerprint, re-measured once the big effect was gone and worth
nothing either way (reserved inserts 6.92 ns against 5.84, growth 2.17 against 2.38, mixed and
within noise). What made the rehash unique is that the reload was the *only* work between
iterations, so it landed directly on the address path of the next random group access; everywhere
else there is enough independent work to hide it.

Checking the two compilers against each other is what found it, and it has a blind spot worth
naming: it can only see a loop where one compiler disambiguates and the other does not. A loop both
serialise looks normal. After the fix no workload shows a branch-specific compiler gap any more --
`build64` is 1.82 ms under clang against 1.88 under gcc, where it was 3.36 against 1.97 -- and the
only large remaining difference, integer iteration at 1.65x slower under gcc, is shared with main
and so is not about this index at all.

**gcc left `probe` out of line, and forcing it inline is the largest single gcc gain on the branch**
(2026-09-05). Found from the full score without SSE2 under gcc, where random integer misses read
0.66 of main while a standalone loop had gcc's SWAR miss *faster* than clang's. `perf` on the
harness binary put 59% of the time in `table::probe<unsigned long>` as its own symbol, called from
`find_all`, where main's lookup was fully inlined: in a large translation unit gcc's unit-growth
budget runs out and the probe, bigger with the SWAR match, is the function it stops inlining. The
symbol exists in the SSE2 binary as well. `ANKERL_UNORDERED_DENSE_FORCEINLINE` on `probe`, paired
against the commit before it: **gcc without SSE2** `rmiss64` 0.66 to 1.23 against main, `rhit64`
1.16 to 1.50, `churn64` 1.19 to 1.48; **gcc with SSE2** `churn64` 1.32, `rhitstr` 1.22, `build64`
1.42; **clang** 1.00 on every workload, with and without SSE2, since it inlined it already. The
whole design assumes the probe is inlined -- the prefetch, the hoisted pointers and the early exit
only pay inside the caller -- so this is the attribute saying what the code already meant. The
full score against main under gcc went from 1.149 to **1.244** with SSE2 (`build64` 1.97,
`buildbig` 1.68) and from 1.097 to **1.165** without, and the gcc string lookups that were the one
workload family behind main are now ahead of it: `rhitstr` 1.13, `rmissstr` 1.05, `findstr` 1.10.

**The gcc string-lookup gap, explained and mostly closed** (2026-09-05). Under gcc the branch's
string lookups measured 0.88-0.91 of main, the one workload family it lost, and the instruction
counts say why: per string hit, main 200 instructions and the branch 224 under gcc, against 235
and 239 under clang, with branch misses identical at 1.8. gcc compiles main's robin hood probe
unusually tightly, and on string keys there are no probe mispredictions for the group index to win
back -- the 1.8 are the hash's length dispatch and `memcmp` -- so the group probe's extra
instructions show undiluted. The assembly is the same shape CLAUDE.md recorded for the old SSE2
probe: gcc spills the loop state, the broadcast fingerprint among it, around the `memcmp` call,
and that plus the prefetches, movemask, tzcnt and the second array are the two dozen. Of those,
five were fixable: building the fingerprint word was an and, a compare, a shift, an or and a
multiply on the critical path of every probe, placement and erase, and a 256-entry table of the
finished words (which the prototype had and the header had lost) makes it one L1 load. Paired,
both compilers: integer misses 1.05-1.06x, `findbig` 1.03x clang and **1.14x gcc**, `rhit64`
1.02x gcc, `ie64` 1.03x gcc, strings 1.00-1.01. Kept. The rest of the string gap under gcc turned out to be the probe not being inlined at all
in that binary -- the entry above this one -- and is closed.

**The whole matrix, run on CI rather than on the desktop** (2026-09-06, `bench.yml`, nine jobs,
12 paired epochs, against `origin/main` and against boost where boost is installable). Geomean of
the fifteen scored workloads:

| machine | vs main | without iteration | vs boost |
|---|---|---|---|
| linux clang SSE2 | 1.22 | 1.29 | 1.48 |
| linux clang no SSE2 | 1.14 | 1.17 | 1.29 |
| linux gcc SSE2 | 1.14 | 1.20 | 1.33 |
| linux gcc no SSE2 | 1.19 | 1.24 | 1.18 |
| arm clang NEON | 1.24 | 1.31 | 1.49 |
| arm clang no NEON | 1.14 | 1.18 | 1.33 |
| arm gcc NEON | 1.22 | 1.28 | 1.48 |
| arm gcc no NEON | 1.04 | 1.05 | 1.26 |
| windows clang SSE2 | 1.12 | 1.15 | not installed |

Ahead of main on every machine and ahead of boost on every machine that has boost, which is the
first time either has been said about anything but one desktop. The desktop's own numbers
(1.21 clang, 1.25 gcc) sit inside this spread, so it was not flattering itself, but the spread is
wider than the run-to-run noise on a quiet machine and these runners are neither quiet nor
identical -- read a column as "which side wins and roughly by how much", not to three digits.

**Only the ARM pair measures what the vector compare is worth**, and reading the x86 pair that way
is a mistake worth naming, because main is in the comparison too. On ARM main has no vector path at
all, so it is the same scalar code in both rows and the difference is entirely this map's: arm gcc
falls from 1.22 to 1.04 without NEON and its lookups go with it (`rhit64` 1.53 to 1.02, `rmiss64`
1.68 to 1.06). On x86 `ANKERL_UNORDERED_DENSE_HAS_SSE2=0` also takes away main's four-bucket SSE2
probe and its vector shifts, so both sides are handicapped and the ratio can move either way --
which is why linux gcc reads *higher* without SSE2 (1.19) than with it (1.14). That number says
main lost more than this map did, not that turning SSE2 off is good.

**Windows had never been measured at all**, and it is the mildest machine of the nine: ahead of
main everywhere except `find64` at 0.98, but with builds at 1.25 where linux clang reads 2.13,
which is what a different allocator does to a workload that grows -- `tame_allocator()` is a glibc
trick and a no-op there.

**The one outlier chased down, and it is the CPU rather than the code**: linux gcc SSE2 iterates at
**0.77** of main, the only figure anywhere below 0.9. Not noise -- err% 0.0, a 76.9-78.6 interval,
and 0.774 then 0.773 on two independent runs with fresh runners. Not the compiler either: built
with the runner's exact package (`Ubuntu 13.3.0-6ubuntu2~24.04.1`) in a container on this desktop,
the same binary reads **1.35**, and upstream gcc 13.4 reads 1.01. Same compiler, same flags, same
source, so the same assembly; the machines are an EPYC 7763 on the runner against a Ryzen 7950X
here, and the answer swings 1.75x between them.

What makes `it64` able to do that is that it is the workload least about the index: 5000 inserts
and 5000 erases against 25 million element visits, so it is almost entirely a vectorisable sum over
the value vector, which is the same `std::vector<std::pair<K, V>>` in both maps. Whichever way the
two loops happen to land, one of them wins by a lot on a given microarchitecture, and nothing about
the group index is being measured. The lesson for reading the matrix is narrow and useful: `it64`
is the column to distrust across machines, and a difference there is not evidence about the index.
Every other workload agrees between the desktop and both runner architectures.

**NEON closed the ARM lookup gap, and it was the whole gap** (2026-09-05). The SWAR row below is
what prompted it: on a Neoverse N2 the branch was 1.11x main overall but *behind* on every lookup,
because the word-at-a-time compare replaces a robin hood probe that was never vectorised on ARM and
so lost nothing to begin with. With `vceqq_u8` and a narrowing shift the same runner gives, main's
time over the branch's, SWAR first and NEON second: `rhit64` 0.91 to **1.48**, `rmiss64` 1.01 to
**1.62**, `find64` 1.03 to 1.26, `findbig` 0.91 to 1.18, `rhitstr` 1.02 to 1.10. Nothing is behind
main any more except `findstr` at 0.97, where the hash rather than the probe is the cost. The score
goes 1.112 to **1.244** and without iteration 1.140 to **1.312**, which puts ARM ahead of where x86
sat before the rehash fix and level with it now. Builds and churn gained too (`build64` 1.67 to
2.08, `churn64` 1.20 to 1.43), since both place through the same compare.

NEON has no movemask and the cheap stand-in does not give the same mask shape: the sixteen answers
land one nibble apart in a 64 bit word, so the mask type and a lane stride are named once and
`first_lane()` divides by the stride. Testing for a match, taking the lowest and clearing it with
`m & (m - 1)` are then written once for all three backends. Guarded to little endian AArch64 --
the mask reads the comparison as one word, and 32 bit ARM has no horizontal ops -- with SWAR, which
is correct everywhere, behind both.

**Before NEON: on ARM the branch was 1.11x main, the same overall as on x86, split the opposite way** (2026-09-05,
`.github/workflows/bench.yml`: the paired harness on a GitHub `ubuntu-24.04-arm` runner, Neoverse
N2, 4 cores, clang 18, 12 interleaved epochs, `origin/main` against the branch, so main's scalar
robin hood probe against the branch's SWAR fingerprint compare -- neither has a vector path there).
Geomean of the scored fifteen **1.112**, without iteration 1.14. Builds are far ahead (`build64`
1.67, `buildbig` 1.54, `buildstr` 1.25) and churn is ahead (`churn64` 1.20, `churnstr` 1.09), but
**lookups are behind main**: `rhit64` 0.914, `findbig` 0.912, `rmissstr` 0.939, `findstr` 0.967,
`find64` 1.03. That is the SWAR match, about 36 instructions for two words where SSE2 does sixteen
bytes in three, paid on every probe, against a robin hood probe that on ARM was never vectorised
either and so lost nothing. It is the number a NEON match is for: `vceqq_u8` plus a narrowing
shift is a handful of instructions, and indivi has the port. Push any branch to `bench` to re-measure this and every other machine in CI; a
`workflow_dispatch` would need the file on main first.

**`ie64` ties boost while executing 58% more instructions, and the counts say why** (2026-09-05,
the workload run on each map alone under `perf stat`, net of its own rng and key scrambling,
`/tmp/ie_count.cpp`):

| per operation | this map | boost |
|---|---|---|
| instructions | 89 | 56 |
| cycles | 43.8 | 43.5 |
| branch mispredictions | 0.61 | 0.67 |
| L1 data misses | 1.7 | 1.2 |
| L2 misses | ~0 | ~0 |

Neither map is instruction-bound: boost retires 1.3 per cycle, this map 2.0, on a core that can do
four. What both wait on is the workload. Every `operator[]` and every `erase` in `ie64` is a coin
flip on whether the key is present -- measured 49.9% hits for both -- so each operation costs about
half a misprediction at ~16 cycles whatever the map, and then one chain of dependent loads that at
10k entries sit in L2: hash, group metadata at a random address, the element to compare. Boost's
chain is metadata then a 16 byte slot; this map's is metadata, index, value, but the index line is
prefetched from the group address before the fingerprints arrive, so the extra hop mostly overlaps.
The 33 extra instructions -- vector append and pop, the counter walks, the second probe on a
successful erase -- run in the shadow of those stalls with issue slots to spare. Boost pays slightly
more in mispredictions because its overflow bits stay set until the next rehash and the table
churns between growths, so its misses walk one group further than a fresh table's.

The same numbers say where the tie ends: on a table that does not fit in cache, or a workload with
no coin flip, the memory chain dominates and boost's shorter one shows -- that is the 11% on
lookups. And nothing here is spare on the instruction side: a change that lengthens the dependent
chain costs at once, a change that only saves instructions on this workload is invisible.

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
  will land in (nothing on `build64`, ~1% worse on `buildstr`). The redundant memset was removed
  on the group index on 2026-09-05 as a simplification, not a speedup: paired on the score it is
  within 1% everywhere, so the ~1% on `iestr` above was layout luck.
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

**`fuzz_group_index` is the one that can reach the index's own structure, and the reason it exists
is worth keeping.** The other targets already hash with an identity over the whole 64 bit key, so
steerability was never what they lacked -- it is *structure*. Filling a group and then emptying it
again means sixteen keys agreeing in their top bits followed by sixteen erases of those same keys,
which a random key stream does not produce, and that is how an unbounded miss survived all of them
plus 767 unit tests. This target splits a key into three bytes the fuzzer chooses separately --
group, identity, fingerprint -- and gives it fill-a-group and erase-a-run as single operations, so
"fill this group, send one key of this class past it, take the fillers back out" is a handful of
mutations rather than a coincidence. Validated by removing the probe's bound and re-running: it
comes back as a libFuzzer timeout inside `probe` within seconds, *from the seed corpus alone*.
`data/fuzz/fuzz_group_index/cb8d5c38...` is that input, kept as a regression seed; note that
coverage minimization drops it, because against the fixed header it is no longer distinctive.

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
scripts/fuzz_afl.py run              # every core, every target, until Ctrl-C
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

One leg builds `--unity=on --unity-size=16`. It is off by default because merging translation units
is bad for development — touching one file recompiles its whole chunk — but it catches a class of
problem separate compilation hides, and an anonymous namespace stops isolating a file once its
neighbours share the chunk. Everything it caught the first time had been there and invisible:
`test/app/print.h` had no include guard, and four `test/bench/*.cpp` each defined a `bench()` that
only became ambiguous when two landed in the same chunk.

**A collision surfaces when the chunking changes, not when the collision is written**, which makes
this leg fail on a commit that has nothing to do with it. `precomputed_hash.cpp` and
`lazy_bucket_allocation.cpp` had each declared a `counting_map` since long before 2026-09-06 —
one counts hash calls, one counts allocations, both in anonymous namespaces — and adding
`test/unit/move_home.cpp` to `test/meson.build` shifted every later file one place and brought the
two into chunk 4 together. So: reproduce this leg with its own size (`meson setup --unity=on
--unity-size=16`, since meson's default is 4 and the boundaries land elsewhere) whenever a test
file is *added or removed*, not only when one is edited. Renamed to `hash_counting_map` rather
than made distinct by chunk luck. The second error cluster in that log, `_Rb_tree_color does not
name a type` inside `stl_tree.h`, is gcc error recovery after the first failure and not a second
bug.

Linters (`scripts/lint/lint-*.py`, all of them via `scripts/lint/all.py`) run in the `lint` job.
Two of them pin their tool, because both tools gain checks or change their output between
releases: `clang-tidy-18` and `clang-format` 21 (`pip install clang-format==21.1.8`).
`lint-clang-format.py` *skips* rather than fails when it cannot find version 21, so a local run
with a different clang-format says so instead of reporting the tree as broken.

## Notes for sandboxed / offline environments

If meson cannot download the wrap subprojects (e.g. GitHub release tarballs blocked), fetch the doctest and fmt sources manually into `subprojects/doctest-2.5.3/` and `subprojects/fmt-12.0.0/` (matching the `directory` field of the `.wrap` files, which is what these version numbers have to keep agreeing with) with a minimal `meson.build` in each that declares `doctest_dep` (header-only, include dir `doctest/`) and `fmt_dep` (include dir `include/`, sources `src/format.cc`, `src/os.cc`) and calls `meson.override_dependency()`. Meson skips the download when the subproject directory already exists. These directories are gitignored — do not commit them.
