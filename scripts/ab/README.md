# Paired A/B measurements

    scripts/ab/run.sh [-r REV] [-b] [-c COMPILER] <workload|all> [epochs]

Builds the working-tree header against `REV`'s (default `HEAD`) in one binary -- the baseline is
renamed into a second namespace -- and runs nanobench's `compare()`: the alternatives run
interleaved in the same slice of time, so drift cancels out of the ratios, and the interval it
prints is about the ratio. `-b` adds `boost::unordered_flat_map` (needs its headers). The
workloads are `test/bench/workloads.h`, the ones `bench_quick_overall_udm` scores -- including
`churn64`/`churnstr`, a table that grows once and then only erases and inserts, and the `*big`
five, a `map<uint64_t, big_value>` whose 64 byte mapped value is what separates a dense map from a
flat one -- plus all-hits and no-hits lookups. Its string keys run from 8 to 135 bytes, skewed towards short; a fixed length
would leave the length dispatch of the hash perfectly predicted. Believe a change when the interval excludes 100%.

## A year of it: 4.8.1 against today (2026-09-06)

`-r` takes any revision, so the harness answers "how far has this come" as easily as "did this
commit help". `3234af2` is where `main` stood on 1 January 2026 -- version **4.8.1**, scalar robin
hood, no vector probe anywhere in it -- against this branch, 12 paired epochs, `-b` for boost:

```sh
scripts/ab/run.sh -r 3234af2 -b all 12 | tee /tmp/year.txt
scripts/ab/summarize.py /tmp/year.txt
```

| geomean over | clang 22 | gcc 16 |
|---|---|---|
| the score, 15 workloads | **1.467** | **1.434** |
| without the three iteration workloads | 1.603 | 1.570 |
| build | 1.883 | 1.612 |
| churn at a fixed size | 1.772 | 1.746 |
| find | 1.476 | 1.551 |
| insert and erase | 1.340 | 1.392 |

Per workload, clang then gcc: `build64` 2.60 and 2.06, `churn64` 2.16 and 2.09, `rhit64` 2.16 and
2.14, `churnbig` 1.99 and 1.93, `rmiss64` 1.98 and 1.76, `buildbig` 1.89 and 1.54, `find64` 1.67 and
1.83, `findbig` 1.64 and 1.78, down through the string workloads at 1.14-1.38 to iteration at
1.00-1.08, which nothing this year touched -- 4.8.1 already iterated a dense vector.

Two things in that run are worth more than the geomean.

**The string hash got slower, and the same table proves it is the hash.** `hashstr` reads 0.96 under
clang and **0.93 under gcc**, where the interval [1.05, 1.09] excludes parity. The control is in the
row below it: boost is handed today's `ankerl::unordered_dense::hash` explicitly, and boost moves
with the candidate (0.92), so this is not the map. The suspicion is that July's wyhash work -- the
independent tail lane for inputs over 48 bytes, and the two 8-byte reads for 8 to 16 -- was tuned
when every string key in the benchmark was exactly 200 bytes, which is a thing this file's own
history says was wrong with the keys and was fixed in September. Not confirmed.

**The lookup gain is mostly at high load, which is why it needs the size sweep to see.** One map per
binary so that nothing shares a translation unit, 20M unreplayed lookups each, nanoseconds per
all-hits `find()`:

| entries | load | 4.8.1 | robin hood (main) | this map |
|---|---|---|---|---|
| 33000 | 0.50 | 7.21 | 6.28 | **4.94** |
| 52000 | 0.79 | 14.26 | 8.79 | **5.95** |
| 132000 | 0.50 | 8.52 | 8.47 | **7.01** |
| 208000 | 0.79 | 16.98 | 10.50 | **7.62** |

At the emptiest a table gets, 4.8.1 is level with main and 1.2-1.5x behind this map; at the fullest
it is 1.6x behind main and 2.2-2.4x behind this map. The scored workloads sit near the top of that
range, which is where a table that grew naturally spends most of its life, and a chart sampled only
at powers of two would show the mild half of it.

**A 50% hit rate is not the average of its parts, and it can order the maps differently from both.**
Paired in one binary, 101 epochs, ms per 200000 lookups at 33000 entries (load 0.50):

| | main | this map | 4.8.1 |
|---|---|---|---|
| all hits | 1.367 | **1.089** | 1.552 |
| all misses | 0.811 | **0.649** | 0.650 |
| 50% hits | 2.037 | 1.753 | **1.725** |

This map is fastest on hits and fastest on misses, and *loses the mix* by 1.6%. The counters say why:
an unpredictable outcome costs a clean probe a fresh half misprediction per lookup -- this map goes
from 0.026 per lookup on hits to 0.545 on the mix -- and costs a probe that already mispredicts 0.6
times on every hit almost nothing, 0.599 to 0.605. It is not that 4.8.1 looks up faster; it is that
it was already paying the bill. Making the benchmark's own hit-or-miss select branchless does not
move any of it, so this is the map's own "did I find it" branch and not the harness's.

The consequence for reading `doc/find_vs_size.svg`: it plots the mix, so at powers of two -- where
the load is 0.5 and this effect is at its largest -- the 4.8.1 line sits lowest, and that must not be
read as "4.8.1 looks up faster". `doc/find_hits_vs_size.svg` is the same sweep with every lookup
hitting, and it settles the question: **4.8.1 is the slowest of the four maps at 164 of the 193
sample points**, is behind this map at every size below 440000, and swings 2.05-2.35x across an
octave where this map swings 1.07-1.28x. At 3251 entries and load 0.79 it costs 10.82 ns against this
map's 3.78. The eleven points where it does come out ahead are all above 440000 entries, where every
map is waiting on memory and the probe hardly matters.

## Lookup cost against table size

`scripts/ab/sweep.cpp` walks the size axis instead of the workload axis: it grows
`map<uint64_t, size_t>` through 193 sizes from 16 to a million entries and times one operation at
each, for the working tree, a baseline revision and boost. `scripts/ab/plot.py` draws the CSV as an
SVG with no dependency beyond the standard library.

```sh
clang++ -O3 -DNDEBUG -std=c++17 -DUDM_AB_HAVE_BOOST -I"$build" -Iinclude -Itest \
    scripts/ab/sweep.cpp "$build/nanobench.o" -o sweep     # $build/base.h as run.sh makes it
# max 2^20 entries, 12 points per octave, 20000 operations per batch, workload, target interval
taskset -c 2 ./sweep 20 12 20000 0 0.02 > doc/find_vs_size.csv        # 3 = all hits, 4 = all misses
scripts/ab/plot.py doc/find_vs_size.csv doc/find_vs_size.svg \
    "Cost of a random find against table size" "nanoseconds per lookup, 50% of them hits"
scripts/ab/plot.py doc/find_vs_size.csv doc/find_ratio_vs_size.svg \
    "How much faster than robin hood" "times faster than the index this replaces, paired" ratio
```

Pin it to a core (`taskset`) and leave the machine alone: a sweep is 40 minutes and every other
thing the machine does lands somewhere in it.

Add `-DUDM_AB_HAVE_JAN` and a second renamed baseline to put a fourth map on the chart; the
namespace is `udmjan` and the sed is `run.sh`'s with `UDMBASE` swapped for `UDMJAN`:

```sh
git show 3234af2:include/ankerl/unordered_dense.h \
    | sed 's/ankerl::unordered_dense/udmjan::unordered_dense/g; s/ANKERL_UNORDERED_DENSE/UDMJAN_UNORDERED_DENSE/g;
           s/namespace ankerl/namespace udmjan/g' > "$build/base_jan.h"
```

It exists because the scored benchmark stops at 200000 entries, whose index is about a megabyte
and lives in cache on any machine that runs it, and the one structural cost of a dense map -- the
value index is a dependent load a flat map does not pay -- is invisible there and large once the
index leaves cache.

The measurement is the scored find workload's: a random lookup with a **50% hit rate**, decided by
an rng of its own rather than alternating, because a predictable sequence of hits and misses is
learned by the branch predictor and stops measuring the branchy part of a probe. The fourth argument
picks the workload: `0` find, `1` churn (an erase and an insert at a fixed size), `2` insert-and-erase
(an `operator[]` and an `erase`, half of each finding nothing), `3` and `4` the same find with every
lookup hitting or every lookup missing, which is what says whether a difference in `0` is about the
lookup or about the unpredictability of its outcome. Both mutating modes erase before they
insert, because the other order crosses the growth threshold and one operation ends up paying for
rehashing the whole table.

**The measurements are paired, and that is not optional.** Each sample point hands its three
batches to nanobench's `compare()`, which runs them interleaved round after round in one process, so
a clock ramp or a noisy neighbour hits all three equally and cancels out of the ratio. The first two
versions of this file measured main to completion, then this map, then boost, and their numbers were
not reproducible: two runs of identical work disagreed by up to 140%, and one contaminated run put a
whole octave 70% high while looking perfectly smooth.

The rounds are chosen by asking for a precision -- `targetIntervalWidth(0.02)` -- rather than by
naming a count, because the count a precision needs depends on the machine. Each row carries
`relative`, `rel_low` and `rel_high`, rendered straight out of the `CompareResult`.

**Every point carries a confidence interval, and the absolute one and the ratio's are not the same
quantity.** The ratio's interval is about a number from which machine drift cancels, because
whatever the machine did during a round it did to all three alternatives. The absolute interval is
about the median epoch of one alternative on its own, so drift does not cancel: it says how tightly
this run pinned its own median, not how well that median would come back on a different afternoon.
Both are nanobench's distribution-free sign test on the paired rounds -- the absolute one is
`detail::medianInterval` applied to the per-epoch times, since a `CompareResult` only renders the
ratio's.

Which absolute estimator to plot is decided by that. The *fastest* round is the steadier number
between runs -- a machine that drifts slower can only push a measurement up, never below what the
work costs -- but there is no distribution-free interval for a minimum, so a band drawn around it
would belong to a different statistic than the line. The charts therefore plot the median with its
band, and the CSV keeps `ns_min` for anyone who wants the floor. Over two runs of the whole sweep on
a quiet machine, how far apart the same point came out, against the width of the band one of those
runs reports for it:

| | median | p90 | worst | interval width |
|---|---|---|---|---|
| absolute, fastest round | **0.49%** | 1.9% | 6.9% | -- |
| absolute, median round | 0.78% | 2.6% | 9.3% | 1.5% |
| the paired ratio | 0.92% | 2.9% | 6.0% | 3.3% |

Read the table before reading a band. Typically the band is the *more* conservative of the two: 1.5%
of the median where the median itself moved 0.78% between runs, and 3.3% on the ratio where the ratio
moved 0.92%. What neither band bounds is the tail -- a point that came out 9.3% apart is nowhere near
a 1.5% interval -- because a within-run interval measures the epochs of one run and says nothing
about what differs between two. That is the whole content of the caption on the chart: it is
precision, not reproducibility, and the two are unrelated at the point where it matters. Both charts
come out of one run: the CSV carries `ns`, `ns_min`, `ns_low`, `ns_high`, `relative`, `rel_low` and
`rel_high` per row.

**The first sample point of the process is measured twice and the first answer thrown away.** It
read high otherwise, and not by a little: at 16 entries boost came out 19% above its own value at 17
entries, in a run whose intervals are 1.5% wide. Cold caches, a cold allocator and a clock still
ramping are all paid by whoever goes first, and pairing cannot cancel it, because what is cold is the
*point* rather than one of the alternatives.

**A 50% hit rate is the maximum-entropy point of the hit-rate curve, so it is the least
discriminating of the three, and it can inverse-rank two maps.** Cost against hit rate, paired in one
binary, us per 200000 lookups at 33000 entries (load 0.50):

| hits | main | this map | 4.8.1 |
|---|---|---|---|
| 0% | 855 | **666** | 782 |
| 10% | 1130 | **975** | 995 |
| 25% | 1500 | 1314 | **1275** |
| 50% | 2063 | 1736 | **1729** |
| 75% | 1737 | **1437** | 2013 |
| 100% | 1360 | **1125** | 1769 |

Every map peaks at 50%, because that is where the outcome branch is least predictable, and the
roughly half a misprediction per lookup it adds is paid by all of them. That is what compresses the
comparison: at 100% hits this map leads 4.8.1 by 1.57x and at 50% by 1.00x. 4.8.1's window is
25-50% hits *and* a load factor near 0.5, and inside it the margin is 0.4-3%; at load 0.79 this map
wins at every hit rate by 1.3-2.1x.

None of that makes the mixed workload wrong to measure -- a program doing membership tests it cannot
predict really does pay the misprediction, and a benchmark with a *predictable* hit sequence would be
worse, since it lets the predictor learn the probe, which is the replay bug below wearing a different
hat. It makes it wrong to read *alone*. The scored suite already has all three points -- `find64` at
50%, `rhit64` at 100%, `rmiss64` at 0% -- which is why the score never showed this inversion, and it
is why `find_hits_vs_size.svg` now sits beside `find_vs_size.svg` on the size axis.

**The sweep replayed its key sequence, and that made a branchy probe look 2.7x better than it is.**
Found 2026-09-06 when the 4.8.1 line came out *ahead* of this map and nobody believed it. Each
workload seeded its `Rng` inside the timed function, so all 400 epochs looked up the identical 20000
keys in the identical order -- and made the identical hit-or-miss decisions -- which a TAGE-style
predictor learns. Measured in one binary, replayed against a sequence that carries on across epochs,
per 20000 all-hits lookups at 33000 entries: main 118.6 against 120.8 us, this map 104.6 against
96.1, **4.8.1 79.8 against 154.1**. The benefit is almost entirely the branchy scalar probe's,
because that is the only one of the three with branches to mispredict -- 0.575 per lookup against
0.02 for main and 0.024 for this map, counted with `perf` on one-map binaries.

It is the mistake this project's own notes record having fixed once already in the scored find
workload, reintroduced in this tool. What makes the scored workloads safe and the sweep not is the
batch size: `find_all` does 10M lookups per epoch, far past what a predictor can hold, while the
sweep does 20000. The rngs now live in a `state` beside each map and carry on across epochs and
across sample points. Cross-checked afterwards against one-map-per-binary runs of the same
workload, the fixed sweep agrees to 3-8% and orders the four maps identically at every point; the
replayed one did not.

**Pairing does not make a single run trustworthy point by point, and it took two sightings to work
out why.** First one of five find runs had this map alone reading 15-54% high at four adjacent sizes
-- 4.59 ns at 64 entries against 2.86 to 3.03 in the other four -- with intervals 0.5% wide saying
nothing was wrong and the other maps at their normal values. Then, when a fourth map was added,
boost read **4.72 ns at 128 entries against the 2.4-2.6 every other run of the same work gives**,
and stayed wrong from 128 up to about 8192. Same signature both times: one alternative, a stretch of
adjacent points, intervals that notice nothing, gone on re-measurement.

The cause is the sweep's own doing. It used to *grow* the maps through the sample points rather than
rebuild them, so each map's addresses depended on the whole interleaved history of the others'
allocations -- and an unlucky layout, once arrived at, sat there until the next reallocation moved
it. Rebuilding each map from scratch at each point costs about a second across the whole sweep and
makes the points independent: a bad layout now spoils one of them, which reads as a spike that a
second run does not have, rather than as a smooth and plausible stretch of curve. The rule survives
the fix, because nothing makes one run right on its own: read a chart for its shape, and check a
surprising *point* against a second run before believing it.

Nanoseconds per find with a 50% hit rate, the median epoch of the committed run. **Two tables,
because one would lie.** A power of two is the *emptiest* a table ever is -- it has just doubled, so
the load factor is about 0.5:

| entries | load | 4.8.1 | robin hood (main) | this map | boost |
|---|---|---|---|---|---|
| 4K | 0.50 | **7.22** | 8.56 | 7.92 | 7.80 |
| 64K | 0.50 | 9.36 | 11.14 | 9.92 | **9.18** |
| 1M | 0.50 | 42.45 | 50.41 | 40.33 | **32.64** |

And the last sample point before each doubling, which is the *fullest* the same table gets, and where
it spends most of its life:

| entries | load | 4.8.1 | robin hood (main) | this map | boost |
|---|---|---|---|---|---|
| 3251 | 0.79 | 9.79 | 9.90 | **8.16** | 8.76 |
| 26008 | 0.79 | 11.60 | 11.50 | **9.34** | 9.78 |
| 104032 | 0.79 | 14.25 | 13.38 | 10.94 | **10.91** |
| 832255 | 0.79 | 45.50 | 48.43 | 36.09 | **32.27** |

Across a single octave 4.8.1 swings **1.26-1.59x** between its cheapest and dearest point, where main
swings 1.11-1.28x, this map **1.04-1.14x** and boost 1.10-1.22x. Point for point 4.8.1 runs from
0.91x of this map just after a doubling to 1.30x just before one. The lookup work of the past year
did not make the best case much faster; it removed the worst case. And the flatness is what puts this
map ahead of boost at load 0.79 up to about 100000 entries -- 0.93 and 0.95 there -- on the workload
boost otherwise wins.

The sweep stops at 1M entries. Above that a single incremental pass is not reproducible whatever the
pairing, because the result depends on page placement of a multi-gigabyte working set that varies
between runs; measuring that regime needs a fresh process per size.

Three more decisions make the picture say something rather than being a smooth line. **Nothing is
reserved**, so each table grows on its own and its load factor sweeps from about a half to the
maximum and falls back at every doubling; that is the sawtooth, and the dotted verticals are where
this map doubles. **Twelve points per octave**, because a sawtooth sampled once per octave is a
straight line. And **two panels**, one to 64K and one over the whole range, with their own y scales,
which is what a detail view is for.

![cost of a random find against table size](../../doc/find_vs_size.svg)
![cost of a find that hits, against table size](../../doc/find_hits_vs_size.svg)
![how much faster than robin hood](../../doc/find_ratio_vs_size.svg)
![cost of churn against table size](../../doc/churn_vs_size.svg)
![cost of insert and erase against table size](../../doc/insert_erase_vs_size.svg)

What to read off them, and the first thing is **which end of the sawtooth you are looking at**. Over
each fully sampled octave from 1K to 64K, cheapest point to dearest:

| workload | 4.8.1 | robin hood (main) | this map | boost |
|---|---|---|---|---|
| find | 1.26-1.59x | 1.11-1.28x | **1.04-1.14x** | 1.10-1.22x |
| churn | 2.53-3.29x | 1.39-1.98x | **1.06-1.36x** | 2.97-4.79x |
| insert and erase | 2.42-3.17x | 1.25-1.65x | **1.12-1.26x** | 2.27-3.37x |

This map is the flattest line on all three, and on the two that mutate it is not close: boost swings
by up to 4.8x across one octave of churn and this map by 1.36x. Robin hood's probe lengthens with the
load and boost's overflow bits only ever get set, so both are relieved only by growing; the group
index's counters come back down on every erase, which is the property the whole design exists for and
this is the picture of it.

That flatness decides who wins, and the answer changes sign along the way. this/boost at a power of
two (load ~0.5) against the last point before the next doubling (load 0.79), above 1.00 meaning boost
is ahead: at ~4K, find 1.02 and 0.93, churn **1.72 and 0.45**, insert-and-erase **1.47 and 0.57**. So
at the emptiest a table gets boost leads everything -- a dense erase must close the hole it leaves in
the value vector and find the moved element's slot with a second probe, where boost probes once and
marks the slot free. At the fullest, and up to about 100000 entries, this map is **1.9-2.2x ahead of
boost on churn** and 1.5-1.8x on insert-and-erase. Past that the memory chain dominates and boost leads at both
ends. The scored `churn` workload agrees with the full-table end, since it reserves and its round is
erase, insert *and two finds*.

## What the hot paths are bound by (Ryzen 9 7950X, clang 22, default `-march`, 2026-09)

A cost model that predicted every experiment of the SSE2-probe work within a cycle or two:
**cycles per operation ≈ 16 × branch mispredictions + instructions / ~3.5**. The loops are
front-end bound between mispredictions, so both terms matter and nothing else does -- the whole
working set of the benchmark sits in L2.

- **u64 lookups are bounded by branch mispredictions.** The scalar probe branched once per bucket,
  so the *position* of a hit leaked into the branch pattern; on random lookups that was 1.35
  mispredictions per lookup, against 0.42 for boost's grouped scan. The SSE2 probe makes the
  outcome one data-dependent branch regardless of position: 0.70, and 35-80% faster.
- **String workloads are latency bound.** At ~167 instructions and ~110 cycles per lookup the
  reorder buffer holds barely one lookup, so anything on the dependency chain shows directly. The
  vector decision (`movmskps` → `tzcnt` → index load) adds ~10 cycles before the value load can
  issue where the scalar path's predicted branch let it issue at once.
- **Hashing is 33-38% of the string workloads**, measured against an 8 byte hash of the same keys:
  findstr 167 → 108 instructions and 110 → 68 cycles, iestr 187 → 121 and 120 → 80. Keys average
  ~50 bytes.
- **`hashstr` resolves a large change and not a small one.** It is a tight loop with nothing else
  in it, so which of the two hashes the A/B builds gets the better code layout matters more than a
  few percent of hashing. For the six-lane threshold it reported 1.01x *slower* twice, where the
  same two headers in separate binaries came out 2.6% faster in five pairs of five and `findstr`
  and `iestr` both resolved 1.01-1.02x faster. It resolved the short-path return, which was 5%,
  cleanly. Check a small change against the map workloads.

Numbers from that session, paired against main (ms; boost for scale). The string rows predate
2026-09-02: they were measured when every string key was 200 bytes, and the keys now run from 8 to
135. The u64 rows are unaffected.

| workload | main | SSE2 probe | boost |
|---|---|---|---|
| find64 | 78.6 | 48.0 | 40.1 |
| findstr (200 byte keys) | 218 | 210 | 182 |
| ie64 | 62.0 | 65.3 | 57.1 |
| iestr (200 byte keys) | 226 | 212 | 188 |
| rhit64 | 145 | 80 | 50 |
| rmiss64 | 48 | 35 | 28 |

## Where a table that only churns stands (2026-09-03)

Every workload above measures a table that has just been built. `churn` measures one that never
grows again: filled to a fixed size, reserved, then erase-one/insert-one forever. Paired against
`boost::unordered_flat_map` with the same hash:

| workload | udm | boost | |
|---|---|---|---|
| churn64 | 12.10 ms | 9.16 ms | boost ahead 32% |
| churnstr | 30.50 ms | 29.48 ms | boost ahead 3% |

Both gaps are much narrower than the fresh-table ones (`rhit64` is 1.71x, `findstr` 1.07x), and the
reason is visible when the run is broken into rounds at 200000 entries. boost's lookups degrade to
1.31x of their fresh cost over three rounds and then snap back, and the round that repairs them
costs +7ns per operation while its bucket count does not change -- an in-place rehash. This map does
not degrade at all: 0.91-1.00x across two million operations, flat churn cost. That is backward
shift deletion doing what it claims.

With string keys both degrade, to 1.41x and 1.71x, and that part is not the table -- the u64 run
proves this map's table does not degrade. It is the heap the key bodies live on. This map churns
strings 1.28x faster than boost at every round.

A run that measures only a fresh table is therefore systematically flattering to a design that
trades erase quality for lookup speed.

## Where inserting and erasing stand (2026-09-02)

`build64` is the weakest workload the score has, and it was added because nothing in the score grew
a table. Paired against `boost::unordered_flat_map` with the same hash, before the two vector
shifts in `place_and_shift_up` and `erase_and_shift_down` and after, with the integer keys
scrambled (see CLAUDE.md: the old ones hashed to a lattice and had nothing to shift):

| workload | before | after | boost |
|---|---|---|---|
| build64 | 6.88 ms | **6.14 ms** | 4.83 ms |
| buildstr | 11.26 ms | **10.66 ms** | 13.36 ms |
| ie64 | 10.79 M op/s | **11.61 M op/s** | 13.89 M op/s |
| iestr | 4.53 M op/s | **4.70 M op/s** | 5.19 M op/s |

Growth is not the whole of it. Inserting 200000 `uint64_t` keys into a table that already reserved
the room cost 52.4 cycles and 101.5 instructions here against boost's 22.6 and 73.5, and the
difference was **0.61 branch mispredictions per insert against 0.10**; erasing them again cost
63.6 cycles and 0.60 mispredictions against boost's 25.0 and 0.10. Both came from the robin hood
shift. Measured over 8 million of each, 73% shift no bucket, 11% shift one, and the rest spread
out, so "is this bucket occupied", asked once per bucket, is a coin flip the predictor cannot win.
boost never moves an element once placed.

The vector shifts ask it once for four buckets and blend the answer, the same trade the probe
made: 0.24 mispredictions and 46.3 cycles per insert for 126 instructions, 0.26 and 54.5 per erase
for 133. The gain grows as the table gets more chains to shift, and on a table erased from full to
empty it is 27% at 10000 entries and 7% at 200000. What is left of the gap to boost is the second
container and the shifting itself; see the dead ends in CLAUDE.md for what did not work on it.

A harness that erases the same keys in the same order every repetition reports 0.003
mispredictions per erase and no gain from any of this: the predictor learns the order. Shuffle per
repetition.

`perf stat -e cycles,instructions,branch-misses` on a single-workload runner is what separates
"more instructions" from "more mispredictions"; `perf record -e cycles:pp` for annotate.
