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
- nanobench prints per-benchmark `err%`; rerun if it's high (> ~3%). A warning about CPU governor/turbo is normal on non-tuned machines — it just means more noise.
- Other useful benchmarks in `test/bench/` (e.g. `bench_copy`, `bench_game_of_life`, find variants) can be run the same way via `-tc=<name>`; run all with `-ns -ts=bench`. List all test cases with `-ltc`.

## Testing

Any change to `include/ankerl/unordered_dense.h` must pass the unit tests:

```sh
meson test -C builddir/clang_release unit --verbose
# or directly (runs all non-skipped tests):
./builddir/clang_release/test/udm-test
```

## Notes for sandboxed / offline environments

If meson cannot download the wrap subprojects (e.g. GitHub release tarballs blocked), fetch the doctest and fmt sources manually into `subprojects/doctest-2.4.12/` and `subprojects/fmt-11.2.0/` (matching the `directory` field of the `.wrap` files) with a minimal `meson.build` in each that declares `doctest_dep` (header-only, include dir `doctest/`) and `fmt_dep` (include dir `include/`, sources `src/format.cc`, `src/os.cc`) and calls `meson.override_dependency()`. Meson skips the download when the subproject directory already exists. These directories are gitignored — do not commit them.
