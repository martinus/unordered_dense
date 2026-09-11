# CLAUDE.md

`ankerl::unordered_dense::{map, set}` — a single-header C++17 dense hash map. The whole
implementation is `include/ankerl/unordered_dense.h`; tests and benchmarks are in `test/` and build
into one doctest binary, `udm-test`.

**Evidence lives in `notes/index-design.md`.** This file is the rules; that file is the ~150
experiments they came from. Before proposing any optimization, grep it — most ideas that look new
have been measured and rejected, several of them twice.

## The index, in one paragraph

Groups of sixteen, not robin hood, since 2026-09-05. One 88 byte block per group of sixteen slots
(`bucket_type::group`, 5.5 bytes per slot): sixteen one-byte fingerprints compared in one SSE2/NEON
instruction, eight overflow counters, and sixteen `uint32_t` value indices, all in one array.
Quadratic probing over groups. An insert increments the counter of its fingerprint class in every
full group it passes; an erase decrements it again, so **there are no tombstones and no rehash is
ever needed**. `bucket_type::group_big` is the same with 64 bit indices.

Nothing moves once placed, with one exception: a hit found inside a *writing* operation
(`operator[]`, `try_emplace`, `insert`) moves itself back to its home group if there is room
(`move_home`). Without it a churned table drifts — at load 0.76 after 200 turnovers, 1.036 groups
per hit against a fresh 1.031 and 1.061 per miss against 1.052 — and with one writing lookup per
round it churns to *better* than fresh.

Against the robin hood index it replaced (4.11.0) it is 1.10x on the score, 1.45x on churn at a
fixed size, 1.5x on misses, 1.33x on hits, 1.2x on insert-erase, and 5.5 bytes per slot instead of
8. The eleven layouts measured and rejected on the way are in `martinus/ai#3`.

## Build

Meson and ninja are required (`pip install -r requirements.txt`). doctest and fmt are fetched
automatically as meson subprojects.

```sh
CXX="ccache clang++" meson setup --buildtype release builddir/clang_release   # once; benchmarks need this
CXX="ccache clang++" meson setup builddir/clang_debug                         # development
ninja -C builddir/clang_release                                               # after every change
```

Warnings are errors (`werror=true`, `warning_level=3`, `-Wconversion`, `-Wold-style-cast`, …).

## Test

Any change to the header must pass:

```sh
meson test -C builddir/clang_release unit --verbose
./builddir/clang_release/test/udm-test              # or directly
```

The `fuzz` suite inside it replays `data/fuzz/<target>` on every run, on every CI leg.

## Benchmark

The metric is `bench_quick_overall_udm`: fifteen nanobench workloads (iterate-while-modifying,
insert/erase, build-from-empty, churn at a fixed size, random find at 50% hits) over
`map<uint64_t, size_t>`, `map<std::string, size_t>` and `map<uint64_t, big_value>`, reported as a
geometric mean. **Lower is better.**

```sh
./builddir/clang_release/test/udm-test -ns -tc=bench_quick_overall_udm   # -ns because benchmarks are skip()ed
./builddir/clang_release/test/udm-test -ltc                              # list all test cases
./builddir/clang_release/test/udm-test -ns -ts=bench                     # every benchmark, incl. bench_copy etc.
scripts/ab/run.sh                                                        # the paired A/B harness — use this
scripts/ab/run.sh -c g++ -r <rev> -b all 12                              # other compiler / baseline revision
```

`.github/workflows/bench.yml` runs the same harness on nine machines (linux and arm, gcc and clang,
with and without the vector compare, one Windows leg), against `origin/main` and against boost where
boost is installable. Push to `bench` to trigger it; `scripts/ab/summarize.py` turns a run into the
markdown table in the job summary. Those runners are shared, which the paired harness is built for —
but it means the **absolute** times from them mean nothing and are not summarised.

### Rules that stop wasted work

Each of these was learned by getting an answer wrong first; `notes/index-design.md` has the case.

- **Release builds only.** Never benchmark a debug build.
- **Record a baseline on the unmodified code first**, then compare after each change, 2–3 times.
  Treat anything inside run-to-run noise (~1–2%) as no change.
- **Never compare runs made at different times.** A desktop drifts several percent over minutes.
  `scripts/ab/run.sh` interleaves baseline and candidate round by round; believe a result when the
  confidence interval excludes 100%.
- **Never quote a ratio taken at one table size.** Load factor sweeps from ½ to max between
  doublings, and two indexes double at *different* sizes. `run.sh` measures five sizes across an
  octave and reports the geometric mean; `-p 1` restores the old single size. An eleven-slot group
  read 1.384 at one size and 1.039 over the octave; `churn64` read 1.199 and 0.974 — sign reversed.
  Every boost ratio not labelled "octave geomean" is a point measurement and must be re-taken.
- **A prefetch pipeline can be a loss while the data is in cache, so sweep across that boundary
  too.** The lookahead costs 13–17 instructions per element in the three loops measured, paid
  whether or not the prefetch was needed — but only one of the three wants a gate, so measure, do
  not assume. `replace()` was measured at 200k and 2M, both above its crossover, and shipped a
  version that was a fifth slower below 65k. Two sizes on the same side of a cache are one size, and
  the load factor still sweeps underneath: the *same* index size reads 0.75 and 1.05 at two n.
- **The control for "does this pipeline pay" is the same entry point with the ring removed**, built
  as a second binary (`solo.sh`, not the paired harness — removing a ring changes a function's
  size). A caller's own loop of single calls is not it: that carries the call boundary's 15
  instructions and shows the bulk entry point winning everywhere. Only the *time* needs the size
  sweep; the instruction premium is flat, so one `perf` run gives it.
- **The paired harness cannot measure anything that changes inlining, and gets the sign wrong.**
  It compiles both headers into one translation unit, which is exactly when a compiler exhausts its
  inlining budget. For anything touching `always_inline`, a function's size or a template boundary,
  build the score twice as two single-header binaries and alternate them — and settle it on
  **instruction counts**, which neither layout nor drift can move.
- **A paired two-header run decides a 10% question, not a 3% one.** Below that, one map per binary
  with `perf stat`. The `hashstr` control never touches a map and still swings 0.92–1.13 between
  runs; that is the resolution floor.
- **Code layout luck is ±3%**, from edits to code that never executes. Judge micro-optimizations by
  mechanism plus a focused microbenchmark, never by a single sub-benchmark delta.
- **Rerun if nanobench prints `err%` above ~3.** A CPU-governor warning is normal noise.
- **A benchmark with a small per-epoch batch must advance its own randomness.** A replayed key
  sequence is learned by the branch predictor and flatters the branchiest probe by up to 2.7x. This
  mistake has been made twice, in two different tools.
- **Do not edit a shell script while it is running.** bash re-reads the file at its old byte offset.

### Rules the workloads themselves must obey

`test/bench/workloads.h`. Breaking any of these makes the score measure the input instead of the map.

- String keys must vary in length (8–135 bytes, skewed short). One fixed length makes the hash's
  length dispatch perfectly predictable: 0.31 branch misses per hash against 0.01.
- Integer keys must be scrambled through a bijection, never small sequential values — the top bits
  of a multiplied small integer walk a lattice and nothing collides.
- Something must grow the table, something must hold a fixed size and churn, and something must
  carry a mapped value bigger than eight bytes. Each of those was missing once and hid a whole class
  of behaviour.
- `workloads::tame_allocator()` must be called by every workload. Without it a build measures the
  kernel's page fault handler: 38% of `build64`'s cycles were kernel time, and whether it was paid
  depended on what ran earlier in the process.
- A churn loop must draw fresh insert keys, not recycle from a small pool — recycling under-reports
  the drift by half.

Scores from before any of these changed are not comparable with scores after.

## Mutation testing

Coverage says a line ran; `scripts/mutate/mutate.py` says something would have noticed it
misbehaving. It never touches the working tree — every build happens in a throwaway copy.

```sh
scripts/mutate/mutate.py --diff                        # whatever is uncommitted — the everyday mode
scripts/mutate/mutate.py --diff HEAD~1                 # only what that change touched
scripts/mutate/mutate.py --bugs scripts/mutate/bugs/erase-path.txt   # put a specific bug back
scripts/mutate/mutate.py --replace OLD NEW             # one bug, must match exactly once
scripts/mutate/mutate.py --reverse HEAD                # undo a fix, keep today's tests
scripts/mutate/mutate.py --lines 1278-1290 --dry-run   # size a run first
```

- Verdicts are `caught` (a test failed — the number to move), `compiler`, `hang`, `oom`, `survived`.
  **Check the verdict, not just that the block applied**: a `compiler` verdict means the question was
  never asked, and three of the obvious rewrites here are rejected by the compiler rather than a test.
- A mutant that reads one slot past a bucket comes back `survived` without a sanitizer. Re-run
  anything surprising with `--meson-arg=-Db_sanitize=address,undefined`.
- **A stale block in a `bugs/` file makes the tool refuse the whole file, so none of the others are
  checked either.** Five of `invariants.txt`'s went stale silently once. Re-run after renames.
- A block whose replacement contains what it replaces needs `<<< additive` on its fence.
- One mutant costs a full rebuild (~100 CPU-seconds); budget a minute for a handful and an hour for
  a function. `--operators` picks what to change; `deletions` and `bitwise` are the sharp ones here.

**`mutate_core.py` is vendored byte-identically into nanobench and oans.** Change it here, run
`scripts/test_mutate.py`, then copy it to both and update all three `.sha256` files —
`lint-mutate-core.py` fails if this copy moved without its hash. `scripts/test_mutate.py` covers the
cmake and make backends this project never runs, because a backend tested only where it is used is
untested.

## Fuzzing

The `fuzz` suite replays the committed corpora on every test run. The targets go looking:

```sh
CXX=clang++ meson setup builddir/fuzz && ninja -C builddir/fuzz test/fuzz_group_index
./builddir/fuzz/test/fuzz_group_index -max_total_time=60 scratch-dir data/fuzz/fuzz_group_index
scripts/fuzz_afl.py run|sweep|minimize [target]        # AFL++, every physical core; minimize folds findings back
```

- **libFuzzer writes into the *first* corpus directory given.** Keep `data/fuzz/...` second or it
  fills with hundreds of generated files.
- `fuzz_group_index` is the only target that can reach the index's own structure: it hashes with the
  identity, so a key names the group, fingerprint class and identity separately, and "fill this
  group, send one key past it, take the fillers back out" is a handful of mutations rather than a
  coincidence. That is how an unbounded miss survived every other target and 767 unit tests.
- `data/fuzz/fuzz_group_index/cb8d5c38…` is the input that hung the probe before the miss had a
  bound. **Coverage minimization drops it every time** — put it back by hand.
- `afl-fuzz` refuses to start while `/proc/sys/kernel/core_pattern` pipes to a crash handler.

## CI

`.github/workflows/main.yml`, 34 legs: gcc and clang, C++17 and C++23, 32 and 64 bit, libc++,
unity, no-SSE2, sanitizers, valgrind, MinGW, MSVC, macOS arm64, Linux ARM64, modules. Any leg
reproduces as:

```sh
meson setup builddir --force-fallback-for=fmt -Dcpp_std=c++17 <matrix setup_args>
meson test -C builddir --print-errorlogs
```

- **Reproduce the unity leg with its own size (`--unity=on --unity-size=16`) whenever a test file is
  added or removed**, not only when one is edited. Adding a file shifts every later file's chunk and
  can collide two anonymous-namespace names written years apart.
- Linters run via `scripts/lint/all.py`. `clang-tidy-18` and `clang-format` 21 are pinned because
  both tools gain checks between releases; the format linter *skips* rather than fails when it
  cannot find version 21.

## Offline / sandboxed environments

If meson cannot download the wrap subprojects, put the sources in `subprojects/doctest-2.5.3/` and
`subprojects/fmt-12.0.0/` (matching each `.wrap` file's `directory` field) with a minimal
`meson.build` declaring `doctest_dep` and `fmt_dep` and calling `meson.override_dependency()`.
Meson skips the download when the directory exists. Both are gitignored.

## What has already been tried

One line each; the evidence is in `notes/index-design.md`, searchable by these phrases. **This is
not the whole list — it is the ideas most likely to be proposed again.**

*The counters.* One counter per group, folly-style: 0.959 on the score. Sixteen nibbles: 0.986.
Thirty-two two-bit: 0.988. An exact in-home counter: worth 2–3% in cache only. Three fresh hash bits
per probe step: noise. **Eight one-byte counters is the top of that axis** — the point where a
counter is one aligned load and still per-class.

*The probe.* Double hashing instead of the triangular sequence: mechanism works, time worse. A
sliding unaligned window (indivi's): 1–5% of a hit, and it forecloses both counters and the merged
block. Fusing the insert's probe with its placement: more instructions, not fewer. Removing either
index prefetch: a clang win and a bigger gcc loss, so left alone.

*The layout.* Merged 88 byte block: **kept**, 7% of a lookup's instructions and 28% of its dTLB
misses at 4M. A quarter of those blocks span three cache lines, and `prefetch_block` steps
by 64 from the start rather than asking for the first and the last — same instruction count, 3.3%,
because it takes the middle line (counters and fingerprints) over the tail. Naming all three lines
is *slower* than either (#250, `scripts/ab/prefetch_lines.cpp`). Splitting fingerprints from counters: a tie. Cache-line-aligning the indices: 0.993.
A second fingerprint in the index's spare bits: 0.975. 16-bit indices for small maps: 0.986. A
12-slot 64 byte block: 0.9888.

*The values.* A narrower value index, tiny pointers included: bounded at ~9% of memory and no speed
by insertion order itself -- the referrer does not choose the position, so the index carries
log2(n) - 1.44 bits per entry. The one speed idea behind it, a value address the group predicts and
can prefetch, was measured on a layout built to make the prediction exact: 0.82 of a hit at 12M
entries, **0.99 for a string**, and misses 13-24% worse at every size. Closed, #229.
A slot back-pointer per value: pays only for string erase-by-iterator, loses the
score. Distance nibbles with it: 0.959. `realloc` growth: available today via
`AllocatorOrContainer`. A growth factor below 2: 7–13% of steady memory for 9–19% of a build —
a knob, not a default. Not zeroing the index: 1.7% of a large build, and undefined behaviour in the
copy constructor.

*The hash.* Twelve hashes measured against this one; **nothing clean is faster on latency**, which
is what a lookup pays. AES-NI is 28% faster hashing and 9–37% slower in the map. Four latency
tunings of the current hash: worthless, and the microbenchmark that said otherwise had the key's
length on its dependency chain. The hash is done; what a string lookup pays is the probe and the
key compare.

*Compilers.* `always_inline` on `probe` and `do_place_element`: **both kept**, worth 1.244 vs 1.149
under gcc and 17–20% of a build in a caller's translation unit. clang splits the insert path and
gcc does not, which is most of the build difference between them: measured exactly with callgrind,
the split costs **15 instructions of prologue and epilogue per insert** and clang pays the same 15
on boost, so it is the call boundary and not this map's register allocation. **PGO removes all of
it** (96.4 to 69.3 instructions, 34.0 to 17.5 cycles) on both maps. No source change steers it, and
six have been tried; what is left against boost once the boundary is gone is eleven instructions,
and at 4M entries the two are level.

*Still open.* Huge pages (22% of a large lookup, nothing asks for them).
A built-in probe-length statistics facility like boost's. The string erase's ~50 ns second hash.

*Bulk lookups.* `visit(first, last, f)` is 1.07-1.14x over the same batch looked up one key at a
time, past the cache. The larger effect is the caller's: **batching the keys at all is 1.5x**, and a
`prefetch(key)` API that was credited with that was reverted the same day it shipped.

*The four pipelines.* The rehash, the range insert and `replace()`'s dedup each have their own
`pipeline_depth` ring, and the bulk visit has chunks instead (a ring there measured 15% worse);
sharing them costs the rehash 1.9 instructions per element and was rejected. Only `replace()` is
gated, on **index** bytes — counting the values too was measured wrong. The range insert is slightly
negative below a few thousand elements and a clear win above; the visit loses 8% only when every key
hits a map below ~16k, where the same map at a 50% hit rate wins 11%, so its axis is the caller's
hit rate. The rehash is the one that has never been measured below cache. #247.
