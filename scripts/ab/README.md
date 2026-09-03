# Paired A/B measurements

    scripts/ab/run.sh [-r REV] [-b] [-c COMPILER] <workload|all> [epochs]

Builds the working-tree header against `REV`'s (default `HEAD`) in one binary -- the baseline is
renamed into a second namespace -- and runs nanobench's `compare()`: the alternatives run
interleaved in the same slice of time, so drift cancels out of the ratios, and the interval it
prints is about the ratio. `-b` adds `boost::unordered_flat_map` (needs its headers). The
workloads are `test/bench/workloads.h`, the ones `bench_quick_overall_udm` scores -- including
`churn64`/`churnstr`, a table that grows once and then only erases and inserts -- plus all-hits and
no-hits lookups. Its string keys run from 8 to 135 bytes, skewed towards short; a fixed length
would leave the length dispatch of the hash perfectly predicted. Believe a change when the interval excludes 100%.

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
