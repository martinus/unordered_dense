# ankerl::unordered_dense: rules for agent sessions

Every line is a rule, a command or a pointer. The evidence for each is in `notes/index-design.md`
under the grep phrase given.

## Performance work: load the `benchmarking` skill first

Before any benchmark, A/B, perf profile, size sweep, speed or memory claim, edit to `scripts/ab/` or
`test/bench/`, or optimization proposal, load the `benchmarking` skill
(`.claude/skills/benchmarking/`). It holds the harness for each question, the measurement rules,
the calibration constants, and `already-measured.md`, the list of closed ideas. Answering from
memory has produced wrong numbers here more than once.

## Files

| what | where |
|---|---|
| the map, single implementation file | `include/ankerl/unordered_dense.h` (+ `stl.h` = its std includes, split out for `import std`; both are needed to copy the header) |
| opt-in huge page allocator | `include/ankerl/huge_page_allocator.h` (separate: needs `<sys/mman.h>`; never referenced by the map) |
| opt-in file mapping for `map_view` | `include/ankerl/mapped_view.h` (separate: needs `<sys/mman.h>`; never referenced by the map) |
| tests + benchmarks, one doctest binary `udm-test` | `test/unit/*.cpp`, `test/bench/*.cpp`, `test/bench/workloads.h` (the scored workloads), `test/app/` (doctest.h with `TEST_CASE_MAP`, allocator fixtures) |
| evidence, ~140 entries in 16 topic sections | `notes/index-design.md` — grep it before proposing anything; each section opens with **Where it stands** |
| measurement harnesses | `scripts/ab/` — `README.md` there documents `run.sh` (paired), `maps.sh`/`maps_one.sh` (other maps, perf), the rest below |
| mutation testing | `scripts/mutate/mutate.py`, bug files in `scripts/mutate/bugs/` |
| fuzz corpora | `data/fuzz/<target>/`, replayed by the `fuzz` suite on every run |
| linters | `scripts/lint/all.py` |
| CI | `.github/workflows/main.yml` (34 legs), `bench.yml` (nine runners, push to `bench`) |
| benchmarking rules, harness choice, closed ideas | `.claude/skills/benchmarking/SKILL.md`, `already-measured.md` next to it |

## Workflow

- **PR only.** `enforce_admins` is on; direct pushes to main are blocked. Branch, `gh pr create`,
  `gh pr merge N --rebase --delete-branch=false` — rebase merges are the only kind allowed.
- **Merge only when told.** "Merge when green": start `.claude/skills/issues/watch_ci.sh N` with
  `run_in_background` (it waits for the runs on the PR's current head; a loop over `gh pr checks`
  exits at once after a push, before any check exists), then merge only if every leg of
  `gh pr checks N` reads `pass` (34 legs in October 2026); otherwise report. Read the tabular view:
  the JSON `conclusion` is `""` while pending, not null.
- **Stacked PR** (edits lines an open PR adds): branch from that branch, `--base <that-branch>`; after
  the first rebase-merges, `git rebase --onto origin/main <first> <second>`, force-push with lease,
  wait for the re-run. #269→#270.
- One PR per issue; commit message per Templates.
- `/simplify` after a PR is up; fix what it finds directly. It has caught a stale mutation block and
  two claims that outran their evidence.
- Never bare `git stash` — other sessions share the stash. Prefer a WIP commit.
- Do not edit a shell script while it runs (bash re-reads at the old offset).
- Every measured result, negative ones included, goes into `notes/index-design.md` as an entry at
  the end of its topic section (format under Templates). Retract in place, never delete. An entry
  that changes an older one's conclusion adds `**Later (date):**` there. Then run
  `scripts/lint/lint-notes.py --fix` (regenerates the contents list; CI fails on a stale list or a
  broken reference).

## Build, test, lint

```sh
CXX="ccache clang++" meson setup --buildtype release builddir/clang_release   # once; all benchmarking uses this
CXX="ccache g++"     meson setup --buildtype release builddir/gcc_release     # the compiler that does not fold
ninja -C builddir/clang_release && ./builddir/clang_release/test/udm-test   # every header change; ~830 cases
./builddir/clang_release/test/udm-test -tc='huge_page*'                     # filter by test case glob
python3 scripts/lint/all.py            # its clang-tidy is 22 here and flags existing code: expected; the other five must pass
UNORDERED_DENSE_CLANG_TIDY=clang-tidy-18 scripts/lint/lint-clang-tidy.py 2>&1 | grep 'unordered_dense.h.*error:'   # CI's version: must print nothing (it fails parsing gcc 16's libstdc++ but still checks the header)
clang-format -i <file>                 # version 21 pinned; the format linter's verdict is its exit code
```

Warnings are errors (`-Wconversion -Wold-style-cast -pedantic-errors`, MSVC `/W4`). Existing build
dirs: `builddir/{clang_release,gcc_release,san_address,unity16,fuzz,...}`; `ninja -C` any of them.

CI legs to reproduce locally before pushing header or test changes, each caught a green-elsewhere failure:
- 32-bit (gcc and clang): `g++ -m32 -std=c++17 -fsyntax-only -Wall -Wextra -Wconversion -Wold-style-cast -pedantic-errors -Iinclude -Itest -I$(dirname $(find subprojects/doctest-2.5.3 -name doctest.h)) <file>` — clang and MSVC each catch narrowings the other misses.
- `-fno-exceptions` (both compilers) for anything that throws; the header has `ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS()`.
- MSVC (`/W4 /WX`, no local compiler): no `std::getenv` in tests, C4996 (#359 went red on four legs); use `fuzz::detail::env()`. A test's `if` on a constant: `if constexpr`.
- **Unity leg whenever a test file is added or removed**: `ninja -C builddir/unity16 && ./builddir/unity16/test/udm-test` (`--unity=on --unity-size=16`). A new file shifts every later file's chunk and can collide anonymous-namespace names.
- ASan+UBSan (`builddir/san_address`) for anything touching memory or the index.
- Any leg: `meson setup builddir --force-fallback-for=fmt -Dcpp_std=c++<matrix cpp_std, default 17> <matrix setup_args> && meson test -C builddir --print-errorlogs <matrix test_args>`.
- Offline: put sources in `subprojects/doctest-2.5.3/` and `subprojects/fmt-12.0.0/` with a minimal `meson.build` calling `meson.override_dependency()`.

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
structure (it re-finds the unbounded miss, which a review found). `data/fuzz/fuzz_group_index/cb8d5c38…` is the hang input;
coverage minimization drops it — put it back. `afl-fuzz` needs `core_pattern` not piped.
A new target needs two or more corpus files before the suite passes: fuzz, then `-merge=1 data/fuzz/<t> scratch`.
`-fork=N` crashed libFuzzer's *parent* (SEGV at a low address, `crash: 0`) on `fuzz_index_view`; use
`-jobs=16 -workers=16`. ASan misses a read inside a vector's capacity: also fuzz from
`meson setup builddir/fuzz_assert -Dfuzz_sanitizers=true -Dcpp_args=-D_GLIBCXX_ASSERTIONS`, which is
what found #299's two-slots-on-one-value read past the end.

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

## Pitfalls

- **Never `pkill -f <pattern>` inside a Bash call**: the pattern is in that shell's own command line,
  it kills itself (exit 144) and the rest of the command never runs (a commit and push were lost
  in #359). Stop a background task by its pid or with TaskStop.
- **A new public header**: `Version X.Y.Z` line in its banner, add it to `CHECKS` in
  `scripts/lint/lint-version.py`, `#include "unordered_dense.h"` for the version namespace, a unit
  test registered in `test/meson.build` (then the unity leg), CMake installs the directory so nothing
  else changes.

## Templates

- Issue: `## The number` (measured, with the notes entry name) / `## What to build` /
  `## How to measure it` / `## Done means`.
- Notes entry: `### What was tried and what happened`, then `*YYYY-MM-DD · #N ·
  kept|rejected|retracted|superseded|open|method|info · Ryzen 9 7950X, clang 22 and gcc 16, the
  scripts used*`, then tables, then "what this says and does not say". Update the section's
  **Where it stands** if the answer changed.
- Commit: one sentence in the repo's voice, a body with the numbers, `Closes #N`.

## The owner's requests, as phrased

| request | meaning |
|---|---|
| "Do the issues", "work the issues" | the `issues` skill: every open owner issue to merged, then a reflection PR |
| "Ok now 260" | implement issue 260 and open its PR |
| "Run simplify" | `/simplify` on the open PR; fix findings directly, never file them as issues unless told |
| "Merge when green" | Workflow above: watch CI, merge when every leg passes, otherwise report |
| "File 1 2 and 3" | one issue per numbered point in the last reply |
| "Comment that" | post the last reasoning as an issue comment; never comment on an issue unasked |
| "Is it still running?" | check the background job and show its partial output |
| "Would it make sense to ...?" | the measured answer (`benchmarking` skill) and a recommendation, not a plan |
