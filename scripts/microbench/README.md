# microbench — standalone harness + A/B pair runner

Tools for iterating on `unordered_dense` performance without the full meson
build, with a measurement methodology that survives noisy shared/virtualized
machines. Written for (and by) optimization sessions on
`bench_quick_overall_udm`; see `handoff/2026-07-08_1030-map-performance-optimization.md`
for measured results, known dead ends, and hot-path analysis.

## Files

- `micro.cpp` — reimplements the six `bench_quick_overall_udm` workloads
  (identical rng seeds and iteration counts as `test/bench/quick_overall_map.cpp`):
  `ie64`, `iestr` (random insert/erase), `find64`, `findstr` (50% hit-rate find),
  `it64`, `itstr` (iterate while adding/removing). Prints the fastest of N
  in-process repetitions in milliseconds, and **verifies the workload result
  against known checksums** — a correctness-breaking "optimization" exits with
  an error instead of producing a meaningless number.
- `ab.sh` — builds a *baseline* binary from a git ref and a *candidate* binary
  from the working tree, runs them strictly interleaved, and reports pairwise
  wins and mean delta.
- `nanobench_impl.cpp` — nanobench implementation TU (only the `Rng` is used).

## Usage

```sh
# 1. edit include/ankerl/unordered_dense.h in the working tree

# 2. build baseline (from HEAD, or any ref/commit) and candidate (working tree)
scripts/microbench/ab.sh build            # baseline = HEAD
scripts/microbench/ab.sh build main       # baseline = main

# 3. compare on the workload you're targeting (6 pairs, min of 3 reps each)
scripts/microbench/ab.sh run findstr
scripts/microbench/ab.sh run ie64 8 3     # more pairs for smaller effects

# or all six workloads
scripts/microbench/ab.sh all
```

Binaries land in `scripts/microbench/build/` (gitignored). `CXX` and
`CXXFLAGS` are overridable; defaults are `clang++` and `-O3 -DNDEBUG -std=c++17`
(matching the meson release build's baseline `-march`, which matters — see the
handoff doc on `mul` vs `mulx`).

## Interpreting results

- The machine drifts by >10% over minutes and same-binary runs vary ±8%.
  **Only interleaved pairs count.** A change is real when the candidate wins
  (almost) every pair; a mean delta under ~2% without a lopsided win count is
  noise.
- Any edit (even to never-executed code) can shift code layout and move a
  single workload by ±3%. Confirm on the paired full benchmark
  (`udm-test -ns -tc=bench_quick_overall_udm`, baseline binary kept from before
  the rebuild) before believing a micro win.
- Approximate healthy values on a 2.8 GHz shared VM (2026-07): ie64 ≈ 115,
  find64 ≈ 120, it64 ≈ 7, iestr ≈ 440, findstr ≈ 355, itstr ≈ 13 ms.

## Final verification

The micro harness is for iteration speed. Before committing, always run:

```sh
ninja -C builddir/clang_release && ./builddir/clang_release/test/udm-test   # unit tests
./builddir/clang_release/test/udm-test -ns -tc=bench_quick_overall_udm     # official score
```
