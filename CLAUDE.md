# CLAUDE.md — ankerl::unordered_dense, operating rules for agent sessions

Reader: an agent session. Everything here is a rule, a command or a pointer; the evidence behind every
rule is in `notes/index-design.md` under the grep phrase given. Nothing here is narrative.

## Files

| what | where |
|---|---|
| the map, single implementation file | `include/ankerl/unordered_dense.h` (+ `stl.h` = its std includes, split out for `import std`; both are needed to copy the header) |
| opt-in huge page allocator | `include/ankerl/huge_page_allocator.h` (separate: needs `<sys/mman.h>`; never referenced by the map) |
| tests + benchmarks, one doctest binary `udm-test` | `test/unit/*.cpp`, `test/bench/*.cpp`, `test/bench/workloads.h` (the scored workloads), `test/app/` (doctest.h with `TEST_CASE_MAP`, allocator fixtures) |
| evidence, ~170 entries | `notes/index-design.md` — grep it before proposing anything |
| measurement harnesses | `scripts/ab/` — `README.md` there documents `run.sh` (paired), `maps.sh`/`maps_one.sh` (other maps, perf), the rest below |
| mutation testing | `scripts/mutate/mutate.py`, bug files in `scripts/mutate/bugs/` |
| fuzz corpora | `data/fuzz/<target>/`, replayed by the `fuzz` suite on every run |
| linters | `scripts/lint/all.py` |
| CI | `.github/workflows/main.yml` (34 legs), `bench.yml` (nine runners, push to `bench`) |

## Workflow

- **PR only.** `enforce_admins` is on; direct pushes to main are blocked. Branch, `gh pr create`,
  `gh pr merge N --rebase --delete-branch=false` — rebase merges are the only kind allowed.
- **Merge only when told** ("merge when green" = wait for 34/34 in `gh pr checks N`; the JSON
  `conclusion` is `""` while pending, not null — read the tabular view).
- **Stacked PR** (edits lines an open PR adds): branch from that branch, `--base <that-branch>`; after
  the first rebase-merges, `git rebase --onto origin/main <first> <second>`, force-push with lease,
  wait for the re-run. #269→#270.
- One PR per issue; commit message = one sentence in the repo's voice + the numbers + `Closes #N`.
- `/simplify` after a PR is up; fix what it finds directly. It has caught a stale mutation block and
  two claims that outran their evidence.
- Never bare `git stash` — other sessions share the stash. Prefer a WIP commit.
- Do not edit a shell script while it runs (bash re-reads at the old offset). Do not run builds or
  anything heavy while a benchmark runs; pin benchmarks with `AB_CORE=2`.
- `AB_BUILD` on `/home/martinus/gra/<name>`, never `/tmp` (tmpfs; a full `/tmp` wedges every shell
  command). Delete the directory once the numbers are in the notes.
- Every measured result goes into `notes/index-design.md` as an entry (bold opening sentence = what
  was tried and what happened; one index line per entry, same commit), and retractions are written
  in place, never deleted. A negative result is an entry too.

## Session mechanics (what cost turns last time)

- **Long runs: one background call, no polling.** Start it with `run_in_background`, `tee` its
  output to `/home/martinus/gra/<name>/*.txt` (the task's own output file is on `/tmp`), and do
  independent work until the notification. The command cap is 1 hour: split sweeps at ~45 min.
- **Smoke every harness at the smallest size, one round, before the real run.** Three scripts this
  week failed only at run time: `env VAR=x fn` cannot call a shell function; `ls_l1_d_tlb_miss.all_l2_dtlb_miss`
  is not an event; `$!` after `cmd | tee &` is `tee`'s pid.
- **Verify the mechanism is engaged before A/B-ing it.** A setting that did not take effect measures
  as a clean null. Huge pages: `AnonHugePages` in `/proc/<pid>/smaps_rollup` mid-run, or `perf stat
  -e ls_l1_d_tlb_miss.tlb_reload_2m_l2_hit`. Prefetch/inlining: the symbol table (`nm -C | grep -c`)
  and `perf record` of the hot function.
- **CI watcher** (pending shows as `pending` in the tabular view; JSON `conclusion` is `""`):
  `for i in $(seq 1 55); do t=$(gh pr checks N | awk -F'\t' '{print $2}' | sort | uniq -c | awk '{printf "%s=%s ", $2, $1}'); grep -q pending <<<"$t" || { echo "$t"; break; }; sleep 60; done`
  — then `gh pr merge N --rebase --delete-branch=false` only if the user said "merge when green".
- **perf events that exist on this machine** (Zen 4, perf 6.x): `cycles`, `instructions`,
  `branch-misses`, `dTLB-load-misses`, `ls_l1_d_tlb_miss.{all,all_l2_miss,tlb_reload_4k_l2_hit,tlb_reload_2m_l2_hit,tlb_reload_coalesced_page_hit}`.
  Four or more events multiplex; three per run for clean counts. `cpuid` is not installed.
- **A new public header**: `Version X.Y.Z` line in its banner, add it to `CHECKS` in
  `scripts/lint/lint-version.py`, `#include "unordered_dense.h"` for the version namespace, a unit
  test registered in `test/meson.build` (then the unity leg), CMake installs the directory so nothing
  else changes.
- **Templates.** Issue: `## The number` (measured, with the notes entry name) / `## What to build` /
  `## How to measure it` / `## Done means`. Notes entry: `**What was tried and what happened**
  (YYYY-MM-DD, issue #N, Ryzen 9 7950X, clang 22 and gcc 16, the scripts used).` then tables, then
  "what this says and does not say"; index line = the bold sentence. Commit: one sentence, body with
  numbers, `Closes #N`. Comment on an issue only when asked ("comment that").
- **The owner's requests, as phrased:** "Ok now 260" = implement issue 260 and open its PR. "Run
  simplify" = `/simplify` on the open PR, fix findings directly (never file them as issues unless
  told). "Merge when green" = watch CI, merge on 34/34, otherwise report. "File 1 2 and 3" = one issue
  per numbered point in the last reply. "Comment that" = post the last reasoning as an issue comment.
  "Is it still running?" = check the background job and show partial output. Questions ("would it
  make sense to…") want the measured answer and a recommendation, not a plan.

## Build, test, lint

```sh
CXX="ccache clang++" meson setup --buildtype release builddir/clang_release   # once; all benchmarking uses this
CXX="ccache g++"     meson setup --buildtype release builddir/gcc_release     # the compiler that does not fold
ninja -C builddir/clang_release && ./builddir/clang_release/test/udm-test   # every header change; ~830 cases
./builddir/clang_release/test/udm-test -tc='huge_page*'                     # filter by test case glob
python3 scripts/lint/all.py            # clang-tidy-18 cannot run on this machine: its failure is expected, the other four must pass
clang-format -i <file>                 # version 21 pinned; the format linter's verdict is its exit code
```

Warnings are errors (`-Wconversion -Wold-style-cast -pedantic-errors`, MSVC `/W4`). Existing build
dirs: `builddir/{clang_release,gcc_release,san_address,unity16,fuzz,...}`; `ninja -C` any of them.

CI legs to reproduce locally before pushing header or test changes, each caught a green-elsewhere failure:
- 32-bit (gcc and clang): `g++ -m32 -std=c++17 -fsyntax-only -Wall -Wextra -Wconversion -Wold-style-cast -pedantic-errors -Iinclude -Itest -I$(dirname $(find subprojects/doctest-2.5.3 -name doctest.h)) <file>` — clang and MSVC each catch narrowings the other misses.
- `-fno-exceptions` (both compilers) for anything that throws; the header has `ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS()`.
- **Unity leg whenever a test file is added or removed**: `ninja -C builddir/unity16 && ./builddir/unity16/test/udm-test` (`--unity=on --unity-size=16`). A new file shifts every later file's chunk and can collide anonymous-namespace names.
- ASan+UBSan (`builddir/san_address`) for anything touching memory or the index.
- Any leg: `meson setup builddir --force-fallback-for=fmt -Dcpp_std=c++17 <matrix setup_args> && meson test -C builddir --print-errorlogs`.
- Offline: put sources in `subprojects/doctest-2.5.3/` and `subprojects/fmt-12.0.0/` with a minimal `meson.build` calling `meson.override_dependency()`.

## The index, in one paragraph

Groups of sixteen since 2026-09-05: one 88 byte block per group (`bucket_type::group`, 5.5 B/slot) =
16 one-byte fingerprints compared in one SSE2/NEON instruction + 8 overflow counters + 16 `uint32_t`
value indices; `group_big` = 152 bytes with 64-bit indices. Quadratic probing over groups. An insert
increments the counter of its fingerprint class in every full group it passes, an erase decrements
it: **no tombstones, no rehash ever needed**. Nothing moves once placed, except a hit found inside a
*writing* operation moves home if there is room (`move_home`), which takes back a churned table's
drift. Every key/value search is bounded (`delta == m_group_mask`); the two placement walks are
unbounded by design (free-slot invariant). Erase swaps the last value into the hole and re-finds
its slot by hashing it (`repoint_value`) — the "string erase second hash", #266. Against the robin
hood index it replaced (4.11.0): 1.10x score, 1.45x churn, 1.5x misses, 1.33x hits, 5.5 B/slot vs 8.

## Benchmark

The metric: `bench_quick_overall_udm`, fifteen workloads (iterate-while-modifying, insert/erase,
build, churn at a fixed size, find at 50% hits) × (`map<uint64_t,size_t>`, `map<string,size_t>`,
`map<uint64_t,big_value>`), geometric mean, **lower is better**. Every workload's table is ≤ 200000
entries; churn is 50000. Anything past L2/L3 is invisible to it and needs the size-axis harnesses.

```sh
./builddir/clang_release/test/udm-test -ns -tc=bench_quick_overall_udm   # the score (-ns: benchmarks are skip()ed)
./builddir/clang_release/test/udm-test -ns -ts=bench                     # every benchmark
```

### Which measurement answers which question

| question | tool | reads |
|---|---|---|
| does a header change move the score, ≥ ~10%, inlining untouched | `scripts/ab/run.sh [-c g++] [-r REV] all 12` (paired, one process, octave geomean) | believe the ratio when the CI excludes 100% |
| anything touching `always_inline`, a function's size, a template boundary, or < 10% | `AB_BUILD=/home/martinus/gra/x scripts/ab/solo.sh [-c g++] [-r main] 5` (one header per binary, alternated) **then** `scripts/ab/perwl.sh /home/martinus/gra/x` | solo.sh prints **baseline/candidate** (>1 = candidate faster; the inverse of run.sh). perwl.sh prints candidate/baseline **ins/op per workload**: deterministic to 4 decimals, the decisive number |
| the mechanism (cycles, misses, TLB) | `perf stat` / `perf record` on the score binary or `scripts/ab/maps_one.sh` (one map per binary) | `perf record` first: an out-of-line symbol in `nm` is not evidence the hot path calls it (#268) |
| an environment setting: page size, allocator tunable, governor | `scripts/ab/same_binary.sh 5 GLIBC_TUNABLES=glibc.malloc.hugetlb=1` | same binary, no layout band; the paired harness cannot see this (both sides share a process) |
| the size axis, other maps, allocators | `scripts/ab/huge_pages.sh`, `scripts/ab/maps.sh`, `scripts/ab/prefetch_lines.sh`, ... (all self-contained, variant = template instantiation, interleaved rounds, medians) | never one size: sweep, and sweep across the cache boundary |
| a ratio against a map that is not this header (boost, another slots-per-group) | `scripts/ab/run.sh -b -p 50` (2.4x the default's wall clock) | five points are worth up to 26% there and 0.6% against another revision of this header: the two sawtooths are only in phase in the second case (#274) |
| how this map stands against every map a caller might pick, in their *own* default configurations | `AB_CORE=2 AB_BUILD=/home/martinus/gra/x scripts/ab/bench_readme.sh` (1h40m, 17 maps x 2 key types x 5 workloads; `-w memory` re-takes one panel) | writes `doc/bench_readme.csv` and the README's four SVGs (this map and 4.11.0 against the other libraries, and the opt-in shapes on their own). Every map gets its own hash here, which `maps.sh` deliberately does not do (#277) |
| is the paired harness biased today | `scripts/ab/run.sh -r HEAD` (same header both sides) | one build is one sample of the layout band: `rmissstr` 1.036 then 0.993-1.008, `rhit64` 0.976-1.000, `hashstr` 0.872-1.05. Re-take it per build and retire anything under ~5% |

Calibration constants, all measured: run-to-run drift 1–2%; **code layout luck ±3%** (two runs of the
same two binaries are one layout sample; `-falign-functions=32` re-rolls it: #262 read 0.9975 and
1.0052 on two layouts); `hashstr` control swings 0.92–1.13 in the paired harness (its floor); a
THP-`always` runner scores 2.6–3.6% above a `madvise` one for no reason in the code — record
`/sys/kernel/mm/transparent_hugepage/enabled` before comparing machines; nanobench `err%` > 3 = rerun.

### Rules (each one cost a wrong answer first; grep the phrase)

- Release builds only. Baseline first, then 2–3 comparisons. Never compare runs from different times.
- **Never quote a ratio from one table size.** Load factor sawtooths ½→max between doublings and two
  indexes double at different sizes: 11 slots read 1.384 at one size, 1.039 over the octave; churn64
  reversed sign. Every boost ratio not labelled "octave geomean" is a point measurement. grep "octave".
  **Five points across that octave never land on its peak** (load 0.763 of a maximum of 0.800) and
  read half the amplitude `-p 50` reads (`rmiss64` 1.29x against 1.46x). Five is right for a
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

### Rules the workloads must obey (`test/bench/workloads.h`; break one and the score measures the input)

String keys 8–135 bytes skewed short (fixed length = predictable hash dispatch); integer keys through
a bijection (small sequential ints never collide); something must grow, something must churn at a
fixed size, something must carry a >8-byte value; `tame_allocator()` in every workload (else 38% of a
build is the page-fault handler); churn draws fresh keys (recycling halves the drift). Scores before
and after any workload change are not comparable, and the change is the fix, not a workaround.

## Mutation testing

```sh
scripts/mutate/mutate.py --diff                    # uncommitted changes — the everyday mode
scripts/mutate/mutate.py --diff HEAD~1
scripts/mutate/mutate.py --bugs scripts/mutate/bugs/erase-path.txt
scripts/mutate/mutate.py --lines 1278-1290 --dry-run   # size a run; one mutant ≈ 100 CPU-s
```
- Verdicts: `caught` is the number to move; `compiler` means the question was never asked.
- **A stale block in a `bugs/` file makes the tool refuse the whole file** — re-run after every
  rename; a rename silently disabled 46 blocks once. `<<< additive` when a replacement contains its
  original. Re-run surprises with `--meson-arg=-Db_sanitize=address,undefined` (a one-past read
  survives without it).
- `mutate_core.py` is vendored byte-identically into nanobench and oans: change here, run
  `scripts/test_mutate.py`, copy to both, update all three `.sha256`.

## Fuzzing

```sh
CXX=clang++ meson setup builddir/fuzz && ninja -C builddir/fuzz test/fuzz_group_index
./builddir/fuzz/test/fuzz_group_index -max_total_time=60 scratch-dir data/fuzz/fuzz_group_index   # corpus dir SECOND
scripts/fuzz_afl.py run|sweep|minimize [target]
```
`fuzz_group_index` hashes with the identity and is the only target that reaches the index's own
structure (it found the unbounded miss). `data/fuzz/fuzz_group_index/cb8d5c38…` is the hang input;
coverage minimization drops it — put it back. `afl-fuzz` needs `core_pattern` not piped.

## Already measured — do not propose again without new evidence (grep the phrase in the notes)

- Counters: 1/group 0.959; 16 nibbles 0.986; 32 two-bit 0.988; exact in-home counter 2–3% in cache only. Eight bytes is the top. "counter"
- Group size: 11 slots below 1.00; 24 slots 0.85–1.00 everywhere (dilutes the 8 classes); 12-slot 64 B block 0.9888. Sixteen is a 3-axis optimum. "twenty-four slots"
- Probe: double hashing worse; sliding window 1–5% of a hit and forecloses counters + merged block; fused probe+placement more instructions. "double hashing", "window"
- Prefetch: `prefetch_index` 2 lines from +64 clamped, `prefetch_block` 2 lines from +0 — naming a third line loses (#250, #252); dropping the second is a clang win and a 12% gcc loss; the two value walks want none (#263). "prefetch_lines"
- Layout: merged 88 B block kept (7% of lookup instr, 28% of dTLB at 4M); split fp/counters tie; line-aligned indices 0.993; second fingerprint 0.975; 16-bit indices 0.986. "merged block"
- Values: narrower index / tiny pointers ~9% memory, no speed (#229); slot back-pointer re-tested across the cache boundary and closed (#266): string erase *by key* 0.88-0.92 at every size from 50k to 4M, and a wash on churn because the insert tax is the same size -- score 0.987 clang / 0.973 gcc, +18.6% live bytes for an 8 byte value and +5% for a 64 byte one, where churn loses at every size, `scripts/ab/back_pointer.patch`; distance nibbles 0.959; growth < 2 is a knob; not zeroing the index 1.7% and UB. "tiny pointers", "back-pointer"
- Hash: 12 alternatives, none faster on latency; AES-NI 28% faster hashing, 9–37% slower in the map; short/long split loses; latency tunings worthless (the microbench had the length on the chain). "hash"
- Compilers: `always_inline` on `probe` and `do_place_element` kept (17–20% of a build in a one-map TU; costs the score, which is a 90-TU binary); clang's insert split = 15 instr/insert call boundary, PGO removes it, six source changes did not. `probe_past_home`'s return shape swept and closed (#267): dropping `value_idx` costs 4.1% of the string find's instructions under clang, `found` as a sentinel 1.1%, the integer find unmoved in every cell -- clang already packs the twelve bytes into the two return registers, so there is nothing to save. "probe_result's shape", "call boundary"
- Range insert sizing from `distance()`: 2x and declined (#248) — every version guesses at data the map cannot see. No heuristics about caller data, ever. "birthday"
- Pipelines: 5 rings (rehash, range insert, replace dedup, merge; visit uses chunks), not shared (1.9 instr/element); gates on index bytes, 256 KB and 1 MB, re-fit whenever the gated loop changes (#247, #242, #257). `visit` 1.07–1.14x; batching keys at all is 1.5x. "gate", "visit"
- Erase path round trips: #260 (−5 instr/erase, transfers), #262 (gcc −5%), #268 (`move_home` re-derivation: real, worth 0.0%). "slot number"
- Huge pages: score 2.6–3.6% via glibc tunable; allocator (#231) builds 1.5–1.7x from 200k, churn 1.33x at 800k, nothing at 50k (no 2 MB block), threshold 2 MB not 4; segmented 16 MB segments fastest build; boost gains equally. Not a default. Aliases #271 and the segment size #272 shipped; `bench_readme.sh` reads the build at 1.67x at a million independently of #231, which is two harnesses sharing no code agreeing on one mechanism. "4 KB pages", "size axis"
- Other maps, own hashes, a million entries (#277, the README's graphs): iterate 5.5-13x over every flat map and 110x over `std::unordered_map`; string build-and-destroy 2.1-2.6x over every flat map; integer find 1.37x and churn 1.64x *behind* boost, and `absl` builds integers at 0.82. Huge pages 0.60 on the integer build-and-destroy, `segmented_map` 0.66 on peak memory. "Seventeen maps in their own default configurations"
- **An allocation counter must not be linked into the binary that measures time.** Interposing `malloc` costs a node map 4.4% of an integer build and a dense map 0.0%; an earlier version that kept its own 16 byte header cost 29%, and that version drew a whole published chart (`absl node` build read 4.68, it is 1.46). Gate the interposers (`-DUDM_COUNT_ALLOC`) and give `memory` its own binary. When counting: charge `malloc_usable_size` plus glibc's 8 byte header, not the request, and interpose `mmap`/`munmap` too. "Seventeen maps in their own default configurations"
- **A build panel must say whether it destroys the map**, and the README's does: teardown is 58-62% of a node map's integer lifetime and 6% of this map's, and for strings it is 64-68 ns/entry for every flat map against 14 here (frees in hash order against frees in insertion order). `maps.h`'s `build()` destroys inside the clock; `bench_readme` measures both columns. "Seventeen maps in their own default configurations"
- **Peak memory as resident pages, not bytes requested**: a doubling flat map leaves every superseded array resident in glibc's arena, worth 1.04-1.28x where counted bytes say 0.96-1.10x; a node map's two numbers agree to 1%. RSS needs a fork per fill, or the second fill reuses the grown arena and reads too low. "Seventeen maps in their own default configurations"
- Still open: a probe-length statistics facility; the rehash has never been measured below cache.
