# Paired A/B measurements

    scripts/ab/run.sh [-r REV] [-b] [-c COMPILER] <workload|all> [epochs]

Builds the working-tree header against `REV`'s (default `HEAD`) in one binary -- the baseline is
renamed into a second namespace -- and runs nanobench's `compare()`: the alternatives run
interleaved in the same slice of time, so drift cancels out of the ratios, and the interval it
prints is about the ratio. `-b` adds `boost::unordered_flat_map` (needs its headers). The
workloads are `test/bench/workloads.h`, the ones `bench_quick_overall_udm` scores, plus all-hits
and no-hits lookups. Its string keys run from 8 to 135 bytes, skewed towards short; a fixed length
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
- **String workloads are latency bound.** At ~224 instructions and ~116 cycles per lookup the
  reorder buffer holds barely one lookup, so anything on the dependency chain shows directly. The
  vector decision (`movmskps` → `tzcnt` → index load) adds ~10 cycles before the value load can
  issue where the scalar path's predicted branch let it issue at once.
- **Hashing is 32-35% of the string workloads**, measured against an 8 byte hash of the same keys:
  findstr 261 → 170 ms, iestr 252 → 172 ms. That is ~60 of the 224 instructions and ~41 of the
  116 cycles, for keys averaging ~50 bytes.

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

`perf stat -e cycles,instructions,branch-misses` on a single-workload runner is what separates
"more instructions" from "more mispredictions"; `perf record -e cycles:pp` for annotate.
