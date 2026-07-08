# Session Handoff — 2026-07-08 10:30

## Resume prompt
Paste this into a fresh session:
> Read `handoff/2026-07-08_1030-map-performance-optimization.md` and continue the work described there: try to squeeze more performance out of `bench_quick_overall_udm` without repeating the documented dead ends.

## Goal
Squeeze more performance out of `ankerl::unordered_dense::map`, measured by the geometric-mean score of `bench_quick_overall_udm` (lower is better). This session was the *second* optimization pass; the first pass (already merged into `main`) landed the big wins ("Speed up hashing, probing, and erase", wyhash tail lane, memset-clear fix).

## State
- Repo `martinus/unordered_dense`, branch `claude/map-performance-optimization-oauepv`, pushed. Working tree clean.
- Last commits:
  - `cfd5c20` wyhash: two 8-byte reads for 8-16 byte inputs (the only optimization that survived this session; bench-neutral but 17–23 % faster hashing for 4–16-byte keys)
  - `de80b8e` CLAUDE.md: document measured optimization dead ends (short summary of this document)
- **Done**: full profiling of all six sub-benchmarks, seven optimization experiments (six rejected via interleaved A/B, one adopted), 422/422 unit tests pass, paired full-bench runs confirm no regression (baseline 0.0759/0.0766/0.0759 vs final 0.0757/0.0767/0.0764).
- **In progress**: nothing — the session ended at a deliberate stopping point: the benchmark is at a measured local optimum on this hardware/ISA.
- **Not started**: the "untried ideas" list at the bottom.

## The benchmark

`bench_quick_overall_udm` = geometric mean of the median elapsed times of six nanobench workloads, each weighted equally:

| sub-benchmark | key type | approx. time (this VM) | dominated by |
|---|---|---|---|
| iterate while adding/removing | `uint64_t` | ~7.3 ms | user-side `std::vector` iteration (harness, not map code) |
| random insert/erase (~10 k live) | `uint64_t` | ~113–119 ms | ~5–6 dependent L2 accesses per op pair |
| 50 % find (grows to ~50 k) | `uint64_t` | ~119 ms | hash + 2 dependent cache accesses ≈ 10 ns map-side |
| iterate while adding/removing | `std::string` (200 B) | ~13 ms | user-side iteration |
| random insert/erase | `std::string` (200 B) | ~437 ms | **~45 % hashing**, ~15 % malloc/free+copy, ~10 % memcmp |
| 50 % find | `std::string` (200 B) | ~350–365 ms | **~45 % hashing**, memcmp on hits, probe misses |

Key facts established by measurement:

- **Hashing the 200-byte string keys is ~45 % of wall time** of the string sub-benchmarks. Verified by substituting a trivial hasher that reads only the first 8 bytes (valid here because only the first 8 bytes vary): find 362→210 ms, insert/erase 437→237 ms.
- The 200-byte wyhash costs ~42 cycles ≈ 15 ns (latency-chained). It is **uop-throughput-bound**: the build uses default `-march` (baseline x86-64), so the compiler emits `mul` (with rax/rdx register shuffling), not BMI2 `mulx`, and no AVX2. ~2× headroom exists *only* with `-march=x86-64-v3`, which is out of the header's control.
- Harness overhead (rng + key setup, no map calls) of the find benches: ~17 ms u64, ~23 ms string. So u64 map-side find ≈ 10 ns ≈ 28 cycles @2.8 GHz = one multiply + two dependent L2/L3 loads. That is the design floor for an index-indirected dense map.
- The iterate benches are user-side loops over the dense `std::vector` — nothing in the header can change them.
- glibc's `memcmp`/`bcmp` dispatches to AVX2 via ifunc regardless of our `-march`, so calling libc beats any inline SSE2 compare loop.

## Benchmark methodology (IMPORTANT — the VM is very noisy)

Same-binary back-to-back runs vary by ±8 % and the machine drifts >10 % over minutes. Naive before/after comparison **will** produce false positives/negatives. Rules that worked:

1. Build release: `CXX=clang++ meson setup --buildtype release builddir/clang_release && ninja -C builddir/clang_release` (see CLAUDE.md "sandboxed/offline" note if the wrap downloads 403; `git clone` of doctest/fmt into `subprojects/` works even when tarball downloads are blocked; doctest ships its own meson.build, fmt needs a minimal one).
2. **Keep a baseline binary**: copy `builddir/clang_release/test/udm-test` elsewhere before rebuilding.
3. **Interleave A/B pairs** (A B A B A B, ≥5–6 pairs) and count wins; only believe a change that wins (almost) every pair. Mean deltas <2 % without a pair sweep are noise.
4. For per-sub-benchmark iteration, use the committed tools in `scripts/microbench/` (see its README and the appendix): `ab.sh build [<git-ref>]` compiles baseline + candidate binaries in seconds without meson, `ab.sh run <workload>` does the interleaved paired comparison, and the harness verifies result checksums so a correctness-breaking change fails instead of being timed.
5. `perf` is **not available** in this container; `valgrind` (callgrind/cachegrind) is. Callgrind instruction counts *overstate* ILP-friendly code (the hash) and *understate* cache-miss-bound code — always confirm with wall-time experiments (e.g. the trivial-hash substitution above).
6. Judge micro-optimizations by mechanism + focused microbenchmark; any edit can shift code layout and move individual sub-benchmarks ±3 %.

## Experiments and results

### Adopted

**E7 — short-input wyhash path** (`cfd5c20`): for 8–16-byte inputs read two (potentially overlapping) 8-byte words instead of assembling `a`/`b` from four 4-byte reads with shifts/ors; two plain 4-byte reads for 4–7 bytes. This is rapidhash's input handling. Latency-chained microbench: 8.0→6.5 ns for len 6–16 (17–23 %), all other lengths unchanged. **Bench-neutral** (bench strings are 200 B) but a real win for real-world short string keys. Changes hash *values* for 4–16-byte inputs — no test pins them; all 422 tests pass.

### Rejected (do not retry without new evidence — all lost interleaved A/B pair sweeps)

| # | Idea | Mechanism hoped for | Result | Why it failed |
|---|---|---|---|---|
| E1 | Force-inline `wyhash::hash` (`always_inline`) | kill ~8–10 cycles call/prologue overhead per string hash | findstr 353.9 vs 360.1 ms, 5:1 pairs against | icache + register pressure at every call site outweighs the saved call |
| E2 | In `do_erase`: hash the moved (last) element and prefetch its home bucket *before* `erase_and_shift_down` | overlap the fixup-scan cache miss with shift-down work | ie64 114.3 vs 115.4 (6:0 against), iestr 439 vs 449 (4:2 against) | out-of-order execution already overlaps these latencies; the early hash lengthens the critical path |
| E3 | Branchless `do_find` fast path for scalar keys: unconditional key compare (`&` not `&&`) + cmov-selected result for the first 2 probes | remove the unpredictable 50 % hit/miss branch | find64 119 vs **194 ms** (6:0 against) — catastrophic | the speculative `m_values[...]` load doubles cache misses on the ~50 % miss lookups; the caller's `it != end()` branch mispredicts anyway |
| E4 | `__builtin_prefetch(&m_values[bucket->m_value_idx])` at the top of `do_find` | warm the value line while the fingerprint check resolves | find64 119.5 vs 126.4 (5:0), findstr 350 vs 362 (4:0) | hardware speculation already issues the load; the prefetch adds address-computation work and wrong-path traffic |
| E5 | Replace wyhash with rapidhash v3 (cloned upstream, benchmarked head-to-head) | newer hash claims wins | wyhash-here is **faster** ≥24 B: at 200 B latency 15.1 vs 21.9 (rapidhash) / 18.1 (rapidhashMicro) ns | this codebase's wyhash already has a 6-lane 96 B loop + independent tail lane (commit `dac9766`); rapidhash's remainder is a serial dependent-mix chain |
| E6 | (from prior session, re-verified reasoning) larger/different `max_load_factor` default | fewer buckets → better cache | not re-run | at the bench's map sizes (10 k, 50 k) the power-of-two bucket count is identical for lf 0.8 vs 0.9 — no effect, only longer probes |
| E8 | Runtime CPU dispatch of `wyhash::hash(void*, size_t)` to a `target("bmi2")` clone (`mulx`), tried both magic-static fn-pointer and glibc-style constant-init atomic pointer | hash is uop-bound; `mulx` avoids `mul`'s rax/rdx shuffling | ceiling (whole build `-mbmi2`, no dispatch): findstr −2.0 % (5:1); magic-static dispatch: +1.7 % *worse* (6:2 against); atomic-ptr dispatch: −0.7 % tie (4:4 findstr, 4:2 iestr) | `mulx` is worth only ~3–5 % on this VM's cores even in isolation (verified: 38 `mulx` in disasm), ≈2 % in map context, and any dispatch indirection costs ~1.3 cycles/call which eats it; real `ifunc` would be zero-cost but is an ELF/glibc-only portability minefield (no Mach-O/MinGW, ASan + `__builtin_cpu_supports`-before-ctors hazards) — not worth ~0 % measured |

### Analysis dead-ends (rejected by reasoning, not measured — challenge these if you have a new angle)

- **Eliminating the erase fixup rehash** (finding the bucket that points at the moved last element costs a full re-hash of its key — ~40 ms of iestr): requires storing hash bits or a back-pointer per element. The 8-byte bucket (24-bit dist + 8-bit fp + 32-bit idx) has no spare bits; the value array layout (`std::pair<Key,T>`) is public API. `home = current_bucket − dist` is circular (you'd need the bucket you're searching for).
- **SoA bucket split** (fingerprint array + index array, SIMD-scan fingerprints): probe chains average ~1.3–1.6 at lf 0.76, almost always within one cache line already; the split adds a second access on hits and breaks the public `Bucket`/`BucketContainer` API.
- **Sentinel/overflow area to avoid `& mask` wraparound**: robin-hood dist is unbounded (24 bits), so the overflow region can't be sized.
- **String data prefetch before `memcmp`**: the data pointer itself comes from the (cold) pair load — nothing to prefetch from earlier.
- **Avoiding the malloc/copy on string insert or the free on erase**: required by value semantics; glibc tcache already makes the pair ~35 ns.

## Untried ideas for a next session

1. **GCC vs clang codegen diff** on the hot paths — now with hard evidence that it's worth doing: g++ 13 compiles the micro harness's `find64` to **2.5× fewer instructions** (193 M vs 477 M Ir, 26 vs 119 ms wall) with correct results, i.e. genuinely better codegen of the find loop, not a benchmark shortcut. (Caveat: gcc *also* found an algebraic shortcut for `it64` — incremental sum maintenance, 3.4 M vs 72 M Ir — so validate each workload's Ir before trusting a gcc wall-time number.) Diff the gcc vs clang asm of the find64 loop; whatever gcc does (likely unrolling/interleaving independent finds) may be reproducible in clang via source structure.
2. **PGO/BOLT-style layout experiment** — not shippable in a header, but would quantify how much of the remaining gap is branch/layout, i.e. whether more source-level branch-hint work (`LIKELY`/`UNLIKELY` placement) has any headroom at all.
3. `do_try_emplace`'s probe loop still re-loads `m_buckets` data pointer per iteration in some instantiations (stores through `Bucket*` may alias) — inspect asm; a local `mask`/data-pointer copy like `place_and_shift_up` does *might* shave a cycle. (Checked for `do_find` only: clang hoists fine there.)
4. The 48-to-96-byte hash gap: inputs of 49–96 bytes run the 3-lane 48 B loop twice; a dedicated 2×48 B unroll with a flatter fold might help mid-size strings (not the 200 B bench, but `bench_quick_overall` isn't everything).
5. Revisit anything above if the environment changes: a machine with real `perf`, or different cores. Note the `-march`/`mulx` speculation has since been **measured** (E8): on this VM `mulx` is only ~3–5 % on the hash, ~2 % in map context — much smaller than hoped. On other microarchitectures the `mul` vs `mulx` gap may differ; re-measure the E8 ceiling (`CXXFLAGS="-O3 -DNDEBUG -std=c++17 -mbmi2" scripts/microbench/ab.sh build` builds *both* binaries with mulx — instead compile one manually) before re-attempting dispatch.

## Commands

```sh
# deps (offline containers): pip install -r requirements.txt
# subprojects if downloads 403 (see CLAUDE.md):
#   git clone --depth 1 --branch v2.4.12 https://github.com/doctest/doctest subprojects/doctest-2.4.12
#   git clone --depth 1 --branch 11.2.0 https://github.com/fmtlib/fmt subprojects/fmt-11.2.0  (+ minimal meson.build, see CLAUDE.md)
CXX=clang++ meson setup --buildtype release builddir/clang_release
ninja -C builddir/clang_release
./builddir/clang_release/test/udm-test                                    # unit tests (all must pass)
./builddir/clang_release/test/udm-test -ns -tc=bench_quick_overall_udm    # the score (last line, lower is better)
```

## Reflection

### 1. What in the delivered work am I least confident is correct?
The claim in `cfd5c20` that the new 8–16-byte hash path is a strict latency win was measured only with clang 18 on one Ivy-generation virtual Xeon, in a microbenchmark where the candidate inlined but the incumbent did not fully — the 17–23 % figure may shrink on other compilers/CPUs (it should not become a regression: it strictly removes two loads and several shift/or ops). Also, hash *quality* for the changed path was argued by equivalence to rapidhash (SMHasher-passing) rather than re-run through SMHasher myself. Check: run the repo's fuzz tests longer (`-ns -ts=fuzz`) or SMHasher3 against `wyhash::hash` if paranoid.

### 2. What assumptions did I make that I never stated explicitly?
(a) That hash output values are not API — no test pins them and a prior commit (`dac9766`) changed them too, but any downstream user persisting `ankerl::unordered_dense::hash` values across processes/versions will break; (b) that the shared-VM timings generalize — all reject/accept decisions came from one machine class (2.8 GHz virtual Xeon, no turbo control); a conclusion like "prefetching hurts" can flip on a CPU with weaker speculation; (c) that `bench_quick_overall_udm` is the *only* success metric — I did not run `bench_copy`, `bench_game_of_life`, or the find variants after the hash change (they don't use 4–16-byte string keys, so risk is minimal, but it's unverified).

### 3. What is the biggest thing the user may not realize about the broader situation?
The benchmark score is now bounded by things outside the header's control: ~45 % of the string benches is a hash that is uop-bound *because the build targets baseline x86-64*. A `-march=x86-64-v3` build (or a function-multiversioning/`ifunc` dispatch inside the header, like glibc does for memcmp) would buy more than every remaining source-level trick combined. Similarly, the u64 benches are bounded by L2/L3 latency of two dependent loads — that is the price of the dense-iteration design itself, and no probing cleverness will remove it.

### 4. If this work breaks in 3 months, what's the most likely reason?
Not "breaks" but "silently invalidated": the dead-end table is empirical, tied to clang 18 + this VM. A toolchain bump (clang 19/20 changing inlining or `mul` register allocation) or moving CI benchmarks to different hardware could make E1/E2/E4 profitable and E7 neutral. The CLAUDE.md note says "re-test before assuming they still hold" for exactly this reason. Concrete invariant to watch: `do_find`'s speculative-load safety argument (empty buckets have `m_value_idx == 0`, table non-empty ⇒ `m_values[0]` valid) is *not currently relied on* because E3 was reverted — but if someone reintroduces a speculative compare, they must re-derive it.

### 5. Were there any tools, scripts, or hooks that would have reduced my churn this session if they had existed when we started?
Yes: (a) a checked-in **standalone micro harness + A/B pair-runner** — I rebuilt both from scratch; they would have saved ~an hour. This has since been addressed: they are committed at `scripts/microbench/`. (b) **Working `perf`** in the container — callgrind's Ir counts misled twice (hash looked like 62 % of find-str by instructions; wall-time substitution showed 45 %, and cache-miss effects were invisible). A container with `perf_event_paranoid` relaxed would eliminate a whole class of guesswork. (c) A pinned "known-good baseline binary" artifact from the previous session — I had to reconstruct the baseline from git history.

### 6. What could the user have done differently to make this session smoother?
State up front that a previous optimization session had already run on this exact metric and landed its wins (I discovered it from git log after setting up; knowing it immediately would have set expectations and directed me to the untried areas faster). Also, a decision in advance on whether bench-neutral-but-real-world improvements (like the short-key hash) are in scope would have removed hesitation — I guessed "yes" based on "you can change anything".

### 7. If I could add one unrequested, industry-leading feature, what would it be?
Runtime ISA dispatch for the hash: compile `wyhash::hash` additionally with `__attribute__((target("bmi2,avx2")))` and select via ifunc/`cpuid` once at startup. No other header-only hash map does this; it would give every user of the default toolchain flags (~everyone) the `mulx` hash for free — on this session's numbers, plausibly ~10–20 % on string-heavy workloads, bigger than anything else left on the table. Needs care (MSVC fallback, constexpr paths, binary-size), but it's a contained, testable feature.

**Update (same day, follow-up session): tried and rejected — see E8.** The plausible "~10–20 %" turned out to be ~3–5 % hash-only / ~2 % in map context on this VM, and safe dispatch indirection consumes it entirely. The idea only makes sense revisited with (a) a CPU where `mul` is much worse than `mulx`, or (b) an AVX2-*vectorized* hash algorithm (different output values) rather than a recompile of the scalar one.

## Appendix — micro harness and A/B pair runner

Both tools are now **committed** at `scripts/microbench/` (see its README):

- `scripts/microbench/micro.cpp` — the six workloads with result-checksum verification, prints min-of-reps ms
- `scripts/microbench/ab.sh` — `ab.sh build [<git-ref>]` compiles baseline (from git ref) + candidate (working tree) binaries in seconds without meson; `ab.sh run <workload> [pairs] [reps]` does the interleaved paired comparison and win counting; `ab.sh all` covers all six workloads

Typical loop: edit the header → `scripts/microbench/ab.sh build main` → `scripts/microbench/ab.sh run findstr` → confirm survivors on the paired full benchmark and unit tests.
