# CLAUDE.md

Guidance for working on `unordered_dense` — a single-header C++17 dense open-addressing hash map/set (`ankerl::unordered_dense::{map, set}`).

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

The main performance metric is `bench_quick_overall_udm`. It runs six nanobench benchmarks covering the most important primitives — iterate-while-modifying, random insert/erase, and random find (50% hit rate) — each for both `map<uint64_t, size_t>` and `map<std::string, size_t>`, then prints the geometric mean of the median elapsed times:

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
- On noisy/shared machines, don't compare runs made at different times — the machine can drift by >10% over minutes. Instead keep a baseline binary around (copy `udm-test` elsewhere before rebuilding) and run baseline and candidate **interleaved** (A B A B A B), then compare paired runs. A change is real when it wins in (almost) every pair.
- Beware code-layout luck: any edit (even to never-executed code) can shift alignment and move individual sub-benchmarks by ±3%. Judge micro-optimizations by mechanism plus a focused microbenchmark, and confirm on the paired geomean, not on a single sub-benchmark delta.
- nanobench prints per-benchmark `err%`; rerun if it's high (> ~3%). A warning about CPU governor/turbo is normal on non-tuned machines — it just means more noise.
- Other useful benchmarks in `test/bench/` (e.g. `bench_copy`, `bench_game_of_life`, find variants) can be run the same way via `-tc=<name>`; run all with `-ns -ts=bench`. List all test cases with `-ltc`.

## Optimization dead ends (verified with interleaved A/B runs; re-test before assuming they still hold)

Measured on a shared x86-64 VM with clang 18, default `-march` (baseline x86-64, so no BMI2/AVX2 in generated code). The `bench_quick_overall_udm` hot paths are close to machine limits: a lookup is hash + two dependent cache accesses (~10 ns map-side), and hashing the 200-byte string keys (~42 cycles each) is ~45% of the wall time of the string sub-benchmarks. Ideas that consistently **regressed** and were reverted:

- Force-inlining `wyhash::hash` into the map (icache/register pressure outweighs saved call overhead).
- A branchless `do_find` fast path for scalar keys (unconditional key compare + conditional-move result): the speculative value load doubles cache misses on the ~50% miss lookups.
- Explicit `__builtin_prefetch` of `m_values[bucket->m_value_idx]` in `do_find`, and computing the moved element's hash early + prefetching its home bucket in `do_erase`: out-of-order execution already hides these latencies.
- Replacing wyhash with rapidhash (v3, 2025): the wyhash implementation here is *faster* for inputs ≥ 24 bytes in both latency and throughput; rapidhash only won at ≤ 16 bytes, and that trick (two plain 8-byte reads instead of building `a`/`b` from four 4-byte reads) has been adopted.

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

The everyday use is putting a *specific* bug back, which is the check that decides whether a new
test earns its place. Bugs worth keeping live in `scripts/mutate/bugs/`:

```sh
scripts/mutate/mutate.py --replace OLD NEW               # one, must match exactly once
scripts/mutate/mutate.py --bugs scripts/mutate/bugs/erase-path.txt
scripts/mutate/mutate.py --reverse HEAD                  # undo a fix, keep today's tests
```

The other mode sweeps for holes nobody thought of, mutating one token at a time. Both compose, and
a change is best asked both questions at once:

```sh
scripts/mutate/mutate.py --diff                          # whatever is uncommitted
scripts/mutate/mutate.py --diff HEAD~1                   # only what that change touched
scripts/mutate/mutate.py --lines 1278-1290,1400 --dry-run
scripts/mutate/mutate.py --bugs bugs.txt --lines 1278-1290 --reuse
```

`--diff` is the everyday mode and measures from the merge base, so a branch that has not caught up
with main does not sweep what main moved on without it.

`--operators` picks what to change, and each can be asked for on its own. Measured over the whole
header, they are not equally worth your time:

| operator | mutants | time | killed | by a test | survivors to triage |
|---|---|---|---|---|---|
| `tokens` (default) | 841 | ~47 min | | | |
| `bitwise` | 76 | 4 min | **99%** | 87% | **1** |
| `deletions` | 665 | 14 min | 94% | 30% | 37 |
| `transpositions` | 181 | 7 min | 45% | 23% | 100 |

`bitwise` mutates `^` and `|`, which the token table leaves alone (`&` is three operators sharing a
spelling — bitwise and, address-of, and the reference declarator — and only a parser can tell them
apart). It is the best value of the four in a header made of masks and fingerprints: four minutes,
and the single survivor is `dist_inc | (hash & fingerprint_mask)` turned into `^`, which is the same
function because the two operands share no bits — as `static_assert(fingerprint_mask < dist_inc)`
right above it guarantees.

`deletions` removes whole statements. Nearly every bug in `bugs/invariants.txt` is a form of "the
code forgot to do this", and none of those is one token. It costs *less* than the token sweep, since
half of them are rejected by the `-fsyntax-only` pre-filter rather than costing a rebuild.

`transpositions` puts two adjacent statements in the other order — the rest of what those
hand-written bugs are, and something `invariants.txt` says outright a sweep cannot express. **Prefer
it with `--diff` rather than over the whole file**: it kills less than half of what it generates, so
a full sweep leaves a hundred survivors to read, most of which are two statements that never touched
the same state. Over a single change it is a handful of mutants and the reading is free. Note also
that it only reaches *adjacent* statements at the *same* indent, so the ordering bug in
`invariants.txt` that moves `pop_back` out of its enclosing `if` is still out of reach.

Mutants that could not have an effect are not generated: comments, string literals and preprocessor
lines are not code, and `std::enable_if_t<..., bool> = true>` is the SFINAE idiom whose value is
never read. Two adjacent `auto` declarations are not transposed either, which is the one rule here
that is a measurement rather than a proof — 24 such pairs over the header, none of them ever caught
— so it says how many it skipped rather than dropping them quietly. A mutant in a branch this configuration does not compile is dropped once the lanes
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

`scripts/test_mutate.py` covers the half of the tool that decides what a verdict *means*, and runs
in CI. It is hermetic: no compiler, no meson, no lanes, no cgroups.

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
