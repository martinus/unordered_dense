---
name: mutation-fuzzing
description: Commands, verdicts and traps for mutation testing (scripts/mutate/mutate.py, the bug files in scripts/mutate/bugs/) and fuzzing (libFuzzer and AFL targets, the corpora in data/fuzz/) of ankerl::unordered_dense. Use before running mutate.py or a fuzzer, before proving a regression test with a mutation, before editing scripts/mutate/ or a bugs/ file, before adding a fuzz target or touching data/fuzz/, and after any change to the probe, the counters or a termination bound.
---

# Mutation testing and fuzzing

Evidence for every rule: `notes/index-design.md`, section "Tooling: tests, mutation testing,
fuzzing, CI, offline builds"; grep the quoted phrase or the issue number.

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
