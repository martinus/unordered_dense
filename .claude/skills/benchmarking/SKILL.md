---
name: benchmarking
description: Picks the harness for a performance or memory question about ankerl::unordered_dense and gives the measurement rules, calibration constants and the list of ideas already measured and closed. Use before running any benchmark, A/B comparison, perf profile or size sweep, before quoting or comparing a speed or memory number, before editing scripts/ab/ or test/bench/, and before proposing or answering "would it make sense to" about an optimization of the index, hash, probe, layout, inlining, allocator or pipelines.
---

# Benchmarking unordered_dense

Evidence for every rule: `notes/index-design.md`; grep the quoted phrase. Before proposing an
optimization, read [already-measured.md](already-measured.md): an idea listed there is closed
unless you bring new evidence.

## The score

The metric: `bench_quick_overall_udm`, fifteen workloads (iterate-while-modifying, insert/erase,
build, churn at a fixed size, find at 50% hits) × (`map<uint64_t,size_t>`, `map<string,size_t>`,
`map<uint64_t,big_value>`), geometric mean, **lower is better**. Every workload's table is ≤ 200000
entries; churn is 50000. Anything past L2/L3 is invisible to it and needs the size-axis harnesses.

```sh
./builddir/clang_release/test/udm-test -ns -tc=bench_quick_overall_udm   # the score (-ns: benchmarks are skip()ed)
./builddir/clang_release/test/udm-test -ns -ts=bench                     # every benchmark
```

## Running a measurement

- No builds or other heavy work while a benchmark runs. Pin with `AB_CORE=2`.
- `AB_BUILD` on `/home/martinus/gra/<name>`, never `/tmp` (tmpfs; a full `/tmp` wedges every shell
  command). Delete the directory once the numbers are in the notes.
- **Long runs: one background call, no polling.** Start it with `run_in_background`, `tee` its
  output to `/home/martinus/gra/<name>/*.txt` (the task's own output file is on `/tmp`), and do
  independent work until the notification. The command cap is 1 hour: split sweeps at ~45 min.
- **A long run's variants are fixed when it compiles, so make them patches or template parameters,
  never "the working tree"**: on 2026-10-02 a trigger edited after a run had built its binaries
  meant a 25-minute restart. `scripts/ab/drift_variants.sh` applies patches to a copy.
- **A timed region under a few milliseconds is noise**: `drift_variants.sh`'s first `miss`/`hit`
  cells timed 5236 lookups (40 us) and read 0.96-1.08 for a layout-only change; 3M lookups made
  them consistent across sizes. Size the reps from time, not from the table.
- **Smoke every harness at the smallest size, one round, before the real run.** Three scripts in
  October 2026 failed only at run time: `env VAR=x fn` cannot call a shell function; `ls_l1_d_tlb_miss.all_l2_dtlb_miss`
  is not an event; `$!` after `cmd | tee &` is `tee`'s pid.
- **A variant of code under development is a template parameter or an API, not a patch**:
  `index_load_nocheck.patch` went stale three times in one afternoon (#299) and became
  `trust::unchecked`. Patches are for variants of code that is not changing.
- **A timed function that creates a map hands it out of the clock to be destroyed, and RSS is
  measured before the timed rounds**: #299's string loads read 14 ns/entry of frees as load time,
  and an RSS after the rounds read 0.2 B/entry for a 27 B/entry map.
- **Verify the mechanism is engaged before A/B-ing it.** A setting that did not take effect measures
  as a clean null. Huge pages: `AnonHugePages` in `/proc/<pid>/smaps_rollup` mid-run, or `perf stat
  -e ls_l1_d_tlb_miss.tlb_reload_2m_l2_hit`. Prefetch/inlining: the symbol table (`nm -C | grep -c`)
  and `perf record` of the hot function.
- **perf events that exist on this machine** (Zen 4, perf 6.x): `cycles`, `instructions`,
  `branch-misses`, `dTLB-load-misses`, `ls_l1_d_tlb_miss.{all,all_l2_miss,tlb_reload_4k_l2_hit,tlb_reload_2m_l2_hit,tlb_reload_coalesced_page_hit}`.
  Four or more events multiplex; three per run for clean counts. `cpuid` is not installed.

## Which measurement answers which question

| question | tool | reads |
|---|---|---|
| does a header change move the score, ≥ ~10%, inlining untouched | `scripts/ab/run.sh [-c g++] [-r REV] all 12` (paired, one process, octave geomean) | believe the ratio when the CI excludes 100% |
| anything touching `always_inline`, a function's size, a template boundary, or < 10% | `AB_BUILD=/home/martinus/gra/x scripts/ab/solo.sh [-c g++] [-r main] 5` (one header per binary, alternated) **then** `scripts/ab/perwl.sh /home/martinus/gra/x` | solo.sh prints **baseline/candidate** (>1 = candidate faster; run.sh reads the same way). perwl.sh prints candidate/baseline **ins/op per workload**: deterministic to 4 decimals, the decisive number |
| the mechanism (cycles, misses, TLB) | `perf stat` / `perf record` on the score binary or `scripts/ab/maps_one.sh` (one map per binary) | `perf record` first: an out-of-line symbol in `nm` is not evidence the hot path calls it (#268) |
| an environment setting: page size, allocator tunable, governor | `scripts/ab/same_binary.sh 5 GLIBC_TUNABLES=glibc.malloc.hugetlb=1` | same binary, no layout band; the paired harness cannot see this (both sides share a process) |
| the size axis, other maps, allocators | `scripts/ab/huge_pages.sh`, `scripts/ab/maps.sh`, `scripts/ab/prefetch_lines.sh`, ... (all self-contained, variant = template instantiation, interleaved rounds, medians) | never one size: sweep, and sweep across the cache boundary |
| a ratio against a map that is not this header (boost, another slots-per-group) | `scripts/ab/run.sh -b -p 50` (2.4x the default's wall clock) | five points are worth up to 26% there and 0.3-1.1% (2.4% on `rhitstr`) against another revision of this header: the two sawtooths are only in phase in the second case (#274) |
| how this map stands against every map a caller might pick, in their *own* default configurations | `AB_CORE=2 AB_BUILD=/home/martinus/gra/x scripts/ab/bench_readme.sh [-u v5.2.0]` (2h45m, 25 maps x 2 key types x 5 workloads; `-u` builds this map's rows from a release; `-w memory` re-takes one panel) | writes `doc/bench_readme.csv` and the README's four SVGs (this map and 4.11.0 against the other libraries, and the opt-in shapes on their own). Every map gets its own hash here, which `maps.sh` deliberately does not do (#277) |
| which parts of the insert to inline and which to call | `scripts/ab/shape_search/run.sh` (gen, score x2, udb3, ch x2, corpus x2, rank; ~3 h, one stage per call) | the rank line: smallest worst ratio per compiler, fixed rule; the Pareto set shows what the alternatives trade |
| any change to what an insert or lookup inlines into its caller, or to the shape of the insert | `AB_CORE=2 AB_BUILD=/home/martinus/gra/x scripts/ab/caller_corpus.sh main HEAD` (9 caller loops x 2 sizes x 2 compilers, one binary each) | the `worst ratio` line per compiler, not the geomean: 5.2.0 won the score and read 3.3x (clang) and 3.6x (gcc) behind the best shape in one loop here. The loops that pick keys with `%` and store into the result can fall into the spill trap ("stored and reloaded") depending on how much is inlined; a change must not add a cliff |
| is the paired harness biased today | `scripts/ab/run.sh -r HEAD` (same header both sides) | one build is one sample of the layout band: `rmissstr` 1.036 then 0.993-1.008, `rhit64` 0.976-1.000, `hashstr` 0.872-1.05. Re-take it per build and retire anything under ~5% |

Calibration constants, all measured: run-to-run drift 1–2%; **code layout luck ±3%** (two runs of the
same two binaries are one layout sample; `-falign-functions=32` re-rolls it: #262 read 0.9975 and
1.0052 on two layouts); `hashstr` control swings 0.87–1.13 in the paired harness (its floor); a
THP-`always` runner scores 2.6–3.6% above a `madvise` one for no reason in the code — record
`/sys/kernel/mm/transparent_hugepage/enabled` before comparing machines; nanobench `err%` > 3 = rerun.

## Measurement rules (each cost a wrong answer first; grep the phrase)

- Release builds only. Baseline first, then 2–3 comparisons. **Never compare runs from different times** -- including a fresh sweep against a stored CSV, which is a systematic few percent and invented a 2-3% big-value regression that `maps.sh -r REV` (both headers, one process, interleaved) showed was 2.3% the other way. `solo.sh -r` cannot reach a header older than the test suite: `fab7984` will not build against today's `huge_page_allocator.h`. grep "Comparing today's runs"
- **A lookup harness here measures throughput, not latency** (`lookups()` draws its key from an rng, so several are in flight). A dependent chain is 1.4x slower for this map at a million entries and **3.0x** for boost, and the two rank the maps oppositely: boost wins the independent hit 1.69x and loses the chain 22.0 to 16.6. Label which one a number is. `scripts/ab/latency.cpp` runs both; its independent loop must not pick keys by reading an array, or it measures that array's misses. grep "measure throughput"
- **A million entries is not past the cache on this machine**: 64 MB of L3 in two 32 MB slices, `AB_CORE` pins to one, and `map<uint64_t, uint64_t>` at 1M is 26 MB here and 32 MB in boost. Two million is the first size clearly out. grep "not past the cache"
- **Never quote a ratio from one table size.** Load factor sawtooths ½→max between doublings and two
  indexes double at different sizes: 11 slots read 1.384 at one size, 1.039 over the octave; churn64
  reversed sign. Every boost ratio not labelled "octave geomean" is a point measurement. grep "octave".
  **Five points across that octave never land on its peak** (load 0.763 of a maximum of 0.800) and
  read about two thirds of the amplitude `-p 50` reads (`rmiss64` 1.29x against 1.46x). Five is right for a
  header-against-header A/B and wrong for anything out of phase with this map. grep "Fifty points".
- **Sweep across the cache boundary for any prefetch/pipeline.** Lookahead costs 13–17 instr/element
  whether needed or not; `replace()` shipped a fifth slower below 65k after being measured only above
  its crossover. Two sizes on one side of a cache are one size. grep "crossover".
- **The control for a pipeline is the same entry point with the ring removed, as a second binary**
  (`solo.sh`), not the caller's loop of single calls (that carries the 15-instruction call boundary).
- **Instruction counts are immune to layout, not to inlining.** A count that moved because the
  inliner moved is real for the shipped binary and transfers to nothing: #254's 133→119.5 vanished
  under `noinline` and never existed under gcc. Before generalising a count: pin with `noinline`,
  check the second compiler, and read `perwl.sh` for workloads that *cannot* run the changed code —
  if they moved, it was the inliner (#268: one `churn` instantiation grew 7065→10384 bytes). grep
  "inlining artifact".
- **A scratch one-map TU can hide a redundancy (#254) or invent one (#268: a `m_group_mask` reload
  real in the scratch TU, hoisted in the score binary).** Read the disassembly of the score binary's
  hot function, found via `perf record`, not a scratch TU's.
- **Profile before disassembling.** #268's only real find had a 0.2% ceiling readable from one
  `perf record`; the audit spent a day reaching it. grep "sweep run over find and churn".
- **The paired harness gets the sign wrong on anything that changes inlining** (both headers in one
  TU exhaust the budget): removing an `always_inline` read 0.979 paired and 1.017 solo. grep "solo".
- **A benchmark that picks its variant inside the timed loop measures the dispatch** (14.11 vs 13.43
  ns for identical addresses). Variant = template parameter; a gated loop = two instantiations, never
  a branch (a predicted branch with a ring, a lambda and an array hanging off it cost 10%).
- **A benchmark that rebuilds its subject every round measures the allocator, bimodally** (4.40 /
  2.85 / 4.76 ns for one binary). Warm the subject, report the median round.
- **A benchmark with a small per-epoch batch must advance its own randomness**; a replayed key
  sequence is learned by the predictor (2.7x flattery; made twice, two tools). grep "replay".
- **A benchmark's input model can make a wrong heuristic look right** (a fresh-key rate that is
  stationary in the harness and 64x wrong on real data). Unit tests judge correctness, benchmarks
  judge speed. grep "birthday".
- **A pipeline's payoff depends on the caller's data, not only the size**: `replace()`'s ring is 1.5x
  with no duplicates and −16% with a quarter, same size. Pick gates on the geomean over key types and
  rates and state what they give up. grep "duplicate rate".
- **Walk value containers with iterators in any loop that also places** — a `uint8_t` fingerprint
  store may alias the container's data pointer, so an indexed read reloads it per element (10.43→2.74
  ns). Hold one cursor fewer than feels natural (`size()`, not `last`): a fourth live value cost 3.5%
  cycles with fewer instructions. grep "cursor".
- **The page size is worth more than anything left in the code**: L1 dTLB = 72 entries = 288 KB;
  every scored workload but `iterate` exceeds it; 1.6 G L2-TLB lookups per score pass, 107x fewer on
  2 MB pages. It is not the map's to set (process policy, source break on the allocator type,
  machine-dependent results): opt-in allocator #231 or the environment. grep "4 KB pages".

## Rules the scored workloads obey (`test/bench/workloads.h`; break one and the score measures the input)

String keys 8–135 bytes skewed short (fixed length = predictable hash dispatch); integer keys through
a bijection (small sequential ints never collide); something must grow, something must churn at a
fixed size, something must carry a >8-byte value; `tame_allocator()` in every workload (else 38% of a
build is the page-fault handler); churn draws fresh keys (recycling halves the drift). Scores before
and after any workload change are not comparable, and the change is the fix, not a workaround.

