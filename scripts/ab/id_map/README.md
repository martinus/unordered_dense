# id_map: a table for integer IDs the caller knows are nearly dense (#379)

`id_map.h` is a prototype, not a public header. It holds four layouts as separate class templates so
that one binary can measure them side by side; `idm::id_map` is the candidate, the others are the
bounds it was measured against.

| class | layout | what it costs |
|---|---|---|
| `paged_map<K, T, B>` | directory of page pointers; a page is a bitmap and one slot per ID | a gap costs a slot: 130 B/entry at 1/16 density |
| `packed_map<K, T, layout::packed>` | 64 byte header per 256 IDs (bitmap, counts per word, values pointer); values packed by rank | the rank: 2x `paged_map`'s lookup on dense IDs |
| `packed_map<K, T, layout::direct / hybrid>` | the same header, a slot per ID / a slot per ID once a block is half full | a header per 256 IDs: a directory 8x `paged_map`'s |
| `id_map<K, T, PageBits = 12, Pairs = false>` | directory entry per 4096 IDs (values pointer, metadata pointer, direct flag); a page is packed until it holds 64 entries that fill half the slots up to its highest ID, then a slot per ID, growing like a vector up to the page; back to packed below an eighth, freed when empty. `Pairs` stores `std::pair<K, T>` and iterators hand out `std::pair<K, T>&` | build 2x `paged_map`; pairs about 50% more memory |

Where each idea came from (the code was read for this, see the notes entry):

- two fixed levels, no tree whose height follows the largest key: Linux `xarray` and Judy show what
  a tree costs per lookup (4-6 dependent loads at 3.5M)
- pages allocated on first write, a shared empty page in place of a null check, values that never
  move when the directory grows: EnTT `sparse_set`/`storage`, flecs' entity index
- a presence bitmap instead of a sentinel value: LLVM `IndexedMap`'s `NullVal` is the counterexample
- values packed by rank under a bitmap, and the representation chosen per node from its own count:
  Judy's `LeafB1` and its cascade
- not taken: EnTT's packed dense array (a third dependent load and insertion order), LLVM
  `SparseSet`'s universe-sized array, swap-and-pop erase (loses ID order), flecs' density-triggered
  fallback to a hash map (a guess about caller data)

Files:

- `check.cpp`: randomized comparison against `std::map` (insert with repeats, `operator[]`, erase,
  find, iteration order, copy, clear) for every layout, `int` and `std::string` values, plus
  web-ifc's ascending pattern, the conversion at a power-of-two top slot, and a drain to empty and refill. Run it under
  ASan+UBSan: `clang++ -std=c++17 -g -O1 -fsanitize=address,undefined check.cpp && ./a.out`.
- `idbench.cpp`: build, a million independent lookups and malloc's bytes per entry for std, this
  repository's map, a vector indexed by ID and the layouts, on dense, web-ifc-like (`1.75 i`) and
  1/16-sparse IDs from 1000 to 3.5M, and churn with IDs drifting upward (`./a.out churn`): `clang++ -O3 -DNDEBUG -std=c++17 -I../../../include idbench.cpp && taskset -c 2 ./a.out`.
- Real programs: `../web-ifc/build.sh` builds a `b-*-idmap` tree with `IfcLoader::_lines` as
  `idm::id_map`; `../redpanda_lb.sh` and `../small_hits.cpp` take `-DWITH_ID_MAP` (through
  `AB_DEFINES` for the former).
