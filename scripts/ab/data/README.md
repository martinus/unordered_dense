# What is in here

The measurements behind the blog post on index structures, so that its charts can be redrawn
without re-running four hours of benchmarks and without a machine that has every other hash map
checked out.

- `maps_u64.csv`, `maps_str.csv`, `maps_big.csv` -- `scripts/ab/maps.sh speed <key>`, the octave
  geometric mean per workload per map, and its ratio to `ankerl::unordered_dense`. Each is the
  **median of five independent runs**, combined with `mapsplot.py merge`. Dropping any one of the
  five moves no integer ratio by more than 4.3%, no big-value ratio by more than 3.0% and no string
  ratio by more than 6.8%, which `merge` prints as its jackknife line. The median rather than a mean
  because one cell needed it: integer `insert/erase` at 32000 read 29.44, 24.52, 26.82, 26.64 and
  26.73 ns, a 20% spread where its neighbours span 2%.
- `maps_memory_*.csv` -- `scripts/ab/maps.sh memory <key>`, median of three runs, **two rows per
  map**: a `memory` row of peak resident bytes per live entry (`max_rss.h`) and an `asked` row of
  peak bytes requested (`count_alloc.h`). They disagree by more than either varies -- resident over
  asked is 0.85-1.04x for a node map, 1.05-1.55x for a dense one and 1.50-1.97x for a flat one -- so
  quote both or say which. There is no n=1000 column: after the instrument's ~128 KB floor is
  subtracted a ~20 KB residual is left, and a thousand-entry map holds 16 KB.

Columns are `key,workload,octave base,map,absolute,relative` -- absolute is nanoseconds per
operation for a speed row and bytes per entry for a memory row, and relative is the ratio to
`unordered_dense` for a speed row and the churned figure for a memory row.

Draw or print them with `scripts/ab/mapsplot.py`:

```sh
scripts/ab/mapsplot.py table scripts/ab/data/maps_u64.csv u64 32000
scripts/ab/mapsplot.py bars  scripts/ab/data/maps_u64.csv u64 32000 out.svg build hit miss churn iterate
```

Ryzen 9 7950X, Fedora, clang 22.1.8, `-O3 -DNDEBUG -std=c++20`, default `-march`, 2026-09-14.
Every map is given this library's hash; `boost-own` and `absl-own` are the same maps with the hash
they ship with, as the control.
