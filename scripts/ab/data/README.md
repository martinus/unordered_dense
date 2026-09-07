# What is in here

The measurements behind the blog post on index structures, so that its charts can be redrawn
without re-running four hours of benchmarks and without a machine that has every other hash map
checked out.

- `maps_u64.csv`, `maps_str.csv`, `maps_big.csv` -- `scripts/ab/maps.sh speed <key>`, the octave
  geometric mean per workload per map, and its ratio to `ankerl::unordered_dense`. Each is the
  geometric mean of **two independent runs**, combined with `mapsplot.py merge`; 372 of the 378
  integer ratios agreed within 5% between the two, worst 1.12.
- `maps_memory_*.csv` -- `scripts/ab/maps.sh memory <key>`: bytes of heap per live entry after a
  build and after a full turnover of churn.

Columns are `key,workload,octave base,map,absolute,relative` -- absolute is nanoseconds per
operation for a speed row and bytes per entry for a memory row, and relative is the ratio to
`unordered_dense` for a speed row and the churned figure for a memory row.

Draw or print them with `scripts/ab/mapsplot.py`:

```sh
scripts/ab/mapsplot.py table scripts/ab/data/maps_u64.csv u64 32000
scripts/ab/mapsplot.py bars  scripts/ab/data/maps_u64.csv u64 32000 out.svg build hit miss churn iterate
```

Ryzen 9 7950X, Fedora, clang 22.1.8, `-O3 -DNDEBUG -std=c++20`, default `-march`, 2026-09-07.
Every map is given this library's hash; `boost-own` and `absl-own` are the same maps with the hash
they ship with, as the control.
