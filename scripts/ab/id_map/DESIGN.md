# `id_map`: a container for integer IDs that are nearly dense

`id_map<K, T>` maps integer IDs to values, for callers who know their keys are IDs: numbers handed out
mostly in order, with gaps. IFC express IDs, raft group IDs, entity IDs, row IDs and graph node IDs
are all like that. It is a prototype for issue #379 and lives in `id_map.h` next to this file. It is
not a public header.

In one sentence: a small directory of 4096-ID pages, where each page stores its values either packed
(a bitmap and a rank, small) or one slot per ID (a bitmap and a direct index, fast), chosen per page
from its own entry count.

What it buys, measured on a Ryzen 9 7950X with clang 22 and gcc 16:

- **web-ifc's IFC loader**: 3-16% less load time than with `std::unordered_map` and 3-17% less than
  with this repository's map 5.3.1 (the most on the two large models), within 1% of a plain vector
  indexed by ID, and the lowest peak memory of all of them.
- **Lookups on dense IDs**: 1.6-5.2x faster than `unordered_dense::map` from 1000 to 3.5M entries,
  close to a plain vector.
- **Memory**: 8.2 bytes per entry for 8 byte values on dense IDs (the map: 24; `std`: 44), 12 bytes
  on IDs that fill only 1/16 of their range.

And what it costs: it is not a hash map. Keys must be IDs, not arbitrary numbers, and several
workloads have a weak spot, listed under [Disadvantages](#disadvantages).

## Why a hash map is the wrong tool for IDs

A good hash spreads keys over the table on purpose, so that clustered keys do not collide. For IDs
that are already 1, 2, 3, ... this destroys the one property that makes them cheap: their order.
Issue #320 measured it in web-ifc, where the loader inserts 3.5M IDs in file order:

| 3.4M IDs inserted, then found, in order | insert | find |
|---|---|---|
| `std::unordered_map` (libstdc++ hashes a `uint32_t` to itself) | 10.6 ns | 1.2 ns |
| `unordered_dense::map` 5.3.1 | 31.7 ns | 12.0 ns |
| `std::unordered_map` given `unordered_dense`'s hash | 94.9 ns | 24.4 ns |

`std` wins because its identity hash writes buckets in key order and every access streams. Give it
a good hash and it is 3x slower than the map. A hash map cannot keep the identity: this map takes
its group from the top bits of the hash, so an order-preserving hash would have to know how many IDs
are coming. The answer is a structure indexed by the ID itself.

## How `id_map` stores its data

`id_map` finds a value with two memory reads: one into a small directory that sits in the cache, one
into the page that holds the value. For 8 byte values it needs 8.2 bytes per entry when the IDs are
dense and 14.3 bytes when only every 1.75th ID exists. `unordered_dense::map` needs 24.1 bytes for
the same data. This chapter builds the design up one step at a time, with one ID as the running
example, and counts every byte on the way.

If you know how a hash map works, you know everything needed here. A hash map turns a key into a
position with a hash function. `id_map` does the same with a much simpler function: the ID *is* the
position. Everything else is about what to do with the gaps.

### A vector is almost the right answer

The simplest container for IDs is a `std::vector<T>` indexed by the ID, with some marker for "this ID
does not exist". A lookup is `values[id]`: one read, no hashing, no probing. In web-ifc this plain
vector loaded the Holter Tower model 17% faster than `std::unordered_map`, which is hard to beat.

Unfortunately the vector has two problems. First, it pays a full slot for every ID that does not
exist. IFC files skip lots of IDs, the largest ID is 1.7 to 2.0 times the number of lines, so 40 to 50% of
the vector is empty. With IDs that use only 1/16 of their range, the vector needed 327
bytes per entry in the microbenchmark. Second, it grows by doubling. When the largest ID crosses the
capacity, the vector allocates twice the size, copies everything, and for a moment holds both copies.

`id_map` keeps the "the ID is the position" idea and fixes the two problems. It cuts the ID range
into pages, and every page decides for itself how to store its values.

### Step 1: an ID is a page number and a slot

`id_map` splits every ID into two parts. The lower 12 bits are the *slot*, the position inside a
page of 4096 IDs. The remaining upper bits are the *page number*. Figure 1 shows the split for ID
10,000, which we will follow through the whole chapter.

```text
ID 10,000 in binary (32 bits):

  0000 0000 0000 0000 0010 | 0111 0001 0000
  upper 20 bits            | lower 12 bits
  page = 10,000 >> 12 = 2  | slot = 10,000 & 4095 = 1808
```

*Figure 1: ID 10,000 lives in page 2, slot 1808.*

Both parts are a shift and a mask, so the split costs nothing. Page 0 holds IDs 0 to 4095, page 1
holds 4096 to 8191, page 2 holds 8192 to 12287, and so on.

### Step 2: the directory finds the page

The directory is a `std::vector` with one entry per page. Each entry is 24 bytes on a 64-bit
machine, laid out as in Figure 2.

```text
byte  0               8               16  17              24
      +---------------+---------------+---+---------------+
      | values (T*)   | meta (meta*)  | d | padding       |
      +---------------+---------------+---+---------------+

values  where the page's values are
meta    the page's bitmap and counters (Step 3)
d       direct flag: is the page stored packed or direct (Step 4)
```

*Figure 2: a directory entry, 24 bytes per 4096 IDs.*

24 bytes per 4096 IDs is 0.006 bytes per ID of the range. For 3.5M dense IDs the whole directory is
855 entries, 21 KB, which means that it stays in the L1 or L2 cache while the program runs. Reading
`directory[2]` for our ID 10,000 is a cache hit practically every time.

Pages that hold no ID at all still have a directory entry, because the directory is indexed by page
number. Their `meta` pointer does not point to nothing, it points to one shared, static, empty `meta`
whose bits are all zero. A lookup in a page that does not exist reads that empty `meta`, finds the bit
not set, and reports a miss. There is no `if (meta == nullptr)` on the lookup path. This trick comes
from EnTT's sparse set.

### Step 3: a bitmap says which IDs exist

Each page that holds at least one ID gets a `meta` block. Figure 3 shows what is in it.

```text
meta, 656 bytes per page

  bits[64]     512 bytes   64 words x 64 bits: bit s is 1 if slot s holds a value
  before[64]   128 bytes   64 x uint16: number of values in the words before word w
  cap            4 bytes   how many values the values array can hold
  top            4 bytes   highest slot ever used, plus 1
  live           4 bytes   number of values in the page
  (padding)      4 bytes
```

*Figure 3: the `meta` block of a page.*

The bitmap answers "does slot s exist?" with one 64-bit word. Slot 1808 is in word 1808 / 64 = 28,
at bit 1808 % 64 = 16. So the lookup for ID 10,000 reads `bits[28]`, shifts it right by 16 and looks
at the lowest bit. If it is 0, the ID does not exist and we are done.

The `before` array is only needed for packed pages, which Step 4 explains. `cap`, `top` and `live`
are bookkeeping for growing, switching and freeing the page.

### Step 4: a page stores its values packed or direct

This is the central idea. A page can store its values in two ways, and Figure 4 shows both for a toy
page of 16 slots in which slots 3, 5, 6 and 9 hold a value.

```text
slot:       15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
bits:        0  0  0  0  0  0  1  0  0  1  1  0  1  0  0  0

packed:  values = [ v3, v5, v6, v9 ]                       4 values
direct:  values = [ -, -, -, v3, -, v5, v6, -, -, v9 ]     a slot for every ID up to 9
```

*Figure 4: the same four values stored packed and direct.*

A *direct* page keeps a slot for every ID, like the vector from the beginning, but only for this
page. The value of slot `s` is simply `values[s]`. The address does not depend on the bitmap at all,
so the CPU can load the value and the bit at the same time. This is as fast as a lookup gets.

A *packed* page keeps only the values that exist, in ID order, without gaps. To find the value of
slot `s`, it counts how many values come before `s`: that count is the position in the array, called
the *rank*. For slot 6 in Figure 4, the set bits below 6 are 3 and 5, so the rank is 2 and the value
is `values[2] = v6`. Counting set bits is one CPU instruction, `popcount`, when the build allows it ([Disadvantages](#disadvantages) has the details).

With 4096 slots, counting all the bits below `s` would mean up to 64 popcounts. This is what the
`before` array is for: `before[w]` already holds the number of values in words 0 to w-1, so the rank
is

```cpp
rank(s) = before[s / 64] + popcount(bits[s / 64] & ((1ull << (s % 64)) - 1))
```

For ID 10,000 in a packed page that is `before[28] + popcount(bits[28] & 0xFFFF)`: one array read and
one popcount. The price is that `before` must be updated on every insert into the page, up to 63
increments, which the compiler turns into SSE2 additions. This rank trick comes from Judy arrays.

So which one is better? It depends on how full the page is. A direct page costs `sizeof(T)` bytes per
slot, used or not. A packed page costs `sizeof(T)` per value plus the rank computation on every
lookup. For a page where most IDs exist, direct wastes little and is faster. For a page where few IDs
exist, direct wastes a lot and packed is the better deal.

### Step 5: every page picks its own layout

A page starts packed. After each insert it checks two conditions: it holds at least 64 values, and
these values fill at least half of the slots up to its highest ID (`2 * live >= top`). When both are
true, the page converts to direct once: it allocates a new array, moves each value from its rank to
its slot, and frees the packed array.

The direct array does not need all 4096 slots right away. It gets the next power of two at or above
`top`, at least 64, and grows like a vector, by doubling, when a higher slot shows up. A table with
only the IDs 0 to 999 becomes one direct page of 1024 slots, not 4096.

Here is what happens with the three ID patterns from the benchmark, inserting in ascending order:

- **Dense IDs (0, 1, 2, ...)**: after 64 inserts `top` is 64, the page is completely full, and it
  goes direct with 64 slots. It doubles to 128, 256, ... up to 4096 as more IDs arrive.
- **web-ifc-like IDs (every 1.75th)**: after 64 inserts `top` is about 112. 2 * 64 = 128 is at least
  112, so the page goes direct too. It ends up with 4096 slots for about 2341 values.
- **Sparse IDs (1/16 of the range)**: after 64 inserts `top` is about 1024, and 128 is much less. The
  page stays packed and ends up with 256 values in a packed array.

Erasing works the other way around. A direct page whose `live` count drops below 1/8 of its slots
converts back to packed, and a page whose last value is erased is freed, its directory entry pointing
to the shared empty `meta` again. The gap between "go direct at 1/2" and "go back at 1/8" is there
so that a page right at the limit does not convert back and forth on every insert and erase.

Note that this decision is made per page, from the values already in that page. The same map can be
direct where the IDs are dense and packed where they are sparse. It never guesses about IDs that have
not been inserted yet.

### A lookup, all steps together

Stripped of templates, `find` looks like this:

```cpp
T* find(uint32_t id) {
    size_t page = id >> 12;
    if (page >= directory.size()) return nullptr;      // past the largest ID
    entry const& e = directory[page];                   // read 1: the directory (cached)
    size_t slot = id & 4095;
    uint64_t word = e.meta->bits[slot / 64];            // read 2a: the bit
    if (((word >> (slot % 64)) & 1) == 0) return nullptr;
    if (e.direct) return &e.values[slot];               // read 2b: the value, in parallel with 2a
    return &e.values[e.meta->before[slot / 64] + popcount(word & ((1ull << (slot % 64)) - 1))];
}
```

For a direct page the CPU needs the directory entry, which is in the cache, and then two independent
reads: the bit word and the value. A vector indexed by ID needs one read. `unordered_dense::map`
needs its hash, a 16 byte fingerprint group, and then the value index and the value.

Surprisingly, how this `if (e.direct)` is written matters. The first version wrote it as
`e.direct ? slot : rank(slot)`, which the compiler turned into a select: it computed the rank on every
lookup, direct page or not, and that read `before` from a second cache line. Written as a branch,
which the CPU predicts correctly as long as most pages are direct, the lookup at 3.5M dense IDs went
from 7.2 to 6.4 ns.

### Where every byte goes

With all parts known, we can compute the memory per entry. Table 1 does it for 1M entries with 8
byte values and compares with what the benchmark measured as malloc's bytes in use.

| bytes per entry, 8 byte values | dense IDs | web-ifc-like (1 in 1.75) | sparse (1 in 16) |
|---|---|---|---|
| page layout | direct | direct | packed |
| values per page of 4096 IDs | 4096 | about 2341 | about 256 |
| values array | 4096 x 8 / 4096 = **8.00** | 4096 x 8 / 2341 = **14.00** | capacity 293 x 8 / 256 = **9.16** |
| `meta` (656 bytes per page) | 656 / 4096 = **0.16** | 656 / 2341 = **0.28** | 656 / 256 = **2.56** |
| directory (24 bytes per page) | **0.006** | **0.010** | **0.094** |
| sum | 8.17 | 14.29 | 11.81 |
| measured | 8.2 | 14.3 | 11.9 |
| `unordered_dense::map` 5.3.1, measured | 24.1 | 24.1 | 24.1 |

*Table 1: computed and measured memory per entry at 1M entries.*

The packed array for 256 values has capacity 293 because packed arrays grow by 25% at a time (4, 5,
6, ... 235, 293), so on average a packed page carries about 12% unused capacity. The remaining 0.1
bytes between the sum and the measurement are malloc's own headers, two allocations per page.

For comparison, `unordered_dense::map` stores each entry as a `std::pair<uint32_t, T>`, 12 bytes, in
a vector that grew by doubling to 1,048,576 entries (12.6 MB). On top of that it has its index: 2^17
groups of 88 bytes each (11.5 MB), sized so the table stays below 80% load. Together that is 24.1
bytes per entry, about two thirds of it in places `id_map` does not need: the key, the index and the
growth slack.

### The bytes it pays where the IDs do not fit

The same arithmetic shows where `id_map` is expensive.

- **A lonely ID.** A page with a single value costs a 656 byte `meta`, a packed array of 4 values
  (32 bytes), two malloc headers and its 24 byte directory entry: about 740 bytes for one entry. If
  your IDs are scattered so that most pages hold only a handful of values, a hash map is much much
  cheaper.
- **A stray large ID.** The directory is indexed by page number, so it must reach the largest page.
  One ID near 2^32 makes the directory 2^32 / 4096 = 1,048,576 entries of 24 bytes: 25 MB, for one
  entry. `id_map` is for IDs, not for arbitrary integers or hash values.
- **`std::pair` iterators.** A standard map's iterator hands out a `std::pair<const K, T>&`. That
  needs the key stored next to every value, so each value becomes 12 bytes instead of 8, and Table 1
  goes to 12.2, 21.3 and 16.5 bytes per entry. The prototype offers both: by default it stores only
  the value and its iterator hands out a small proxy `{key, T&}`, with `Pairs = true` it stores the
  pair.

## Why it is fast

Each item below was measured. The removed alternatives are in the notes entry.

1. **One dependent load after a directory that is always in cache.** A direct lookup is: read the
   directory entry (L1), read `values[slot]`. That is the same chain as a vector indexed by ID; the
   map needs its group, its fingerprint compare and then the value.
2. **No null check on a miss.** Directory entries without a page point to one shared static `meta`
   whose bits are all zero (from EnTT). A miss is the bounds check and one bit test.
3. **A branch, not a select, between direct and packed.** Written as `direct ? s : rank(s)`, the
   compiler emitted a select, which computes the rank and loads its counts on every lookup. A branch
   with `__builtin_expect` removed that: 7.2 to 6.4 ns at 3.5M dense IDs.
4. **Small dense tables become direct too.** The rule counts against the highest ID in the page, not
   against 4096, so IDs 0..999 become a direct page of 1024 slots: 1000 dense lookups went from 3.5 ns
   (packed) to 1.4 ns.
5. **Building costs little.** Values are constructed in place and never moved when the directory
   grows (EnTT, flecs); only packed pages keep their per-word counts, which took a build from 5.1 to
   3.3 ns per ID; the compiler turns the count update into SSE2 `paddw`.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="img/lookups-dark.svg">
  <img alt="Lookup time against table size for dense, web-ifc-like and sparse IDs: id_map and the vector stay at 1.3 to 2.5 ns up to a million dense IDs while the map and std rise to 10 and 16 ns." src="img/lookups-light.svg">
</picture>

A million independent lookups of random present IDs (throughput, not latency), median of five
rounds after a warm-up, clang / gcc, ns per lookup:

| | std::unordered_map | unordered_dense::map 5.3.1 | vector indexed by ID | id_map | id_map, std::pair API |
|---|---|---|---|---|---|
| dense 1,000 | 1.70 / 1.72 | 2.89 / 3.07 | 1.29 / 1.29 | 1.69 / 1.88 | 1.68 / 2.03 |
| dense 16,000 | 2.41 / 2.30 | 3.24 / 3.38 | 1.29 / 1.31 | 1.70 / 1.88 | 1.73 / 2.06 |
| dense 100,000 | 4.44 / 4.05 | 5.11 / 4.91 | 1.69 / 1.68 | 1.98 / 2.08 | 2.24 / 2.31 |
| dense 1,000,000 | 16.45 / 15.04 | 9.53 / 9.14 | 2.10 / 2.25 | 2.44 / 2.61 | 2.54 / 2.65 |
| dense 3,500,000 | 29.66 / 27.21 | 32.18 / 32.17 | 8.65 / 8.80 | 6.15 / 12.73 | 10.70 / 13.33 |
| web-ifc-like 16,000 | 6.52 / 5.37 | 3.46 / 3.61 | 1.47 / 1.55 | 1.83 / 2.14 | 1.84 / 2.41 |
| web-ifc-like 100,000 | 5.43 / 5.16 | 5.13 / 5.26 | 2.05 / 2.15 | 2.55 / 2.66 | 2.62 / 2.86 |
| web-ifc-like 1,000,000 | 22.90 / 20.77 | 11.28 / 10.40 | 5.90 / 7.19 | 3.23 / 3.29 | 3.85 / 4.08 |
| web-ifc-like 3,500,000 | 40.32 / 36.12 | 34.86 / 33.25 | 11.75 / 12.01 | 14.11 / 13.82 | 14.74 / 15.41 |
| sparse 1/16 16,000 | 4.96 / 4.38 | 3.78 / 3.80 | 2.20 / 2.37 | 4.20 / 4.19 | 4.32 / 4.52 |
| sparse 1/16 100,000 | 11.74 / 10.49 | 5.52 / 5.56 | 3.52 / 3.60 | 5.73 / 5.93 | 6.20 / 6.45 |
| sparse 1/16 1,000,000 | 30.40 / 27.15 | 10.17 / 10.90 | 13.56 / 13.69 | 8.39 / 8.50 | 9.46 / 9.68 |

The dense 3.5M cell is the noisy one: the same `id_map` code read 5.5 to 12.7 ns across runs on
this machine, so read that row as "5.5-13 ns, against 32 for the map".

## Why it is memory efficient

- **Packed pages pay only for what exists**: 8 bytes per 8 byte value plus 0.16 bytes per ID of range
  for the bitmap and counts. A sparse region costs about what its entries cost.
- **Direct pages only exist where at least half the slots are used**, so a gap costs at most one slot
  per entry on average.
- **Nothing doubles.** A vector indexed by ID reallocates and copies itself as the largest ID grows,
  and holds old and new arrays at once. Pages are allocated one by one and the directory copies only
  24 byte entries.
- **No hash index.** The map spends 5.5 bytes per slot on its index plus the slack of its load factor.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="img/memory-dark.svg">
  <img alt="Bytes per entry at a million entries: id_map 8.2, 14.3 and 11.9 for dense, web-ifc-like and sparse IDs; the map 24.1 in all three; std 43.6; the vector 12.6, 25.2 and 327." src="img/memory-light.svg">
</picture>

| 1M entries, clang: build ns per ID / bytes per entry | std::unordered_map | unordered_dense::map 5.3.1 | vector indexed by ID | id_map | id_map, std::pair API |
|---|---|---|---|---|---|
| dense | 13.6 / 43.6 | 11.3 / 24.1 | 1.5 / 12.6 | 3.9 / 8.2 | 4.0 / 12.2 |
| web-ifc-like | 18.0 / 43.6 | 11.3 / 24.1 | 3.1 / 25.2 | 4.2 / 14.3 | 4.5 / 21.3 |
| sparse 1/16 | 38.8 / 43.6 | 11.0 / 24.1 | 34.9 / 327.2 | 8.6 / 11.9 | 10.0 / 16.5 |

Memory here is malloc's bytes in use after the build, divided by the entry count.

## In a real program: web-ifc

web-ifc's `IfcLoader::_lines` maps each express ID to its line. Replacing only that member, on four
of web-ifc's public test models, load time (parse and geometry) as a ratio to `std::unordered_map`,
median of eleven interleaved rounds on one pinned core:

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="img/webifc-dark.svg">
  <img alt="web-ifc load time as a ratio to std::unordered_map: id_map 0.837 to 0.971, the vector 0.829 to 0.968, the map 0.969 to 1.104." src="img/webifc-light.svg">
</picture>

| ratio to std, clang / gcc | unordered_dense::map 5.3.1 | vector indexed by ID | id_map | id_map, std::pair API |
|---|---|---|---|---|
| Holter Tower, 177 MB | 0.969 / 0.975 | 0.829 / 0.849 | 0.837 / 0.854 | 0.847 / 0.866 |
| LTU A-House, 181 MB | 1.104 / 1.094 | 0.927 / 0.935 | 0.920 / 0.929 | 0.928 / 0.935 |
| ISSUE_098, 73 MB | 1.002 / 1.000 | 0.968 / 0.972 | 0.971 / 0.974 | 0.976 / 0.978 |
| ISSUE_068, 57 MB | 1.025 / 1.022 | 0.951 / 0.956 | 0.949 / 0.955 | 0.960 / 0.962 |
| peak RSS | 0.904-0.948 | 0.914-0.984 | 0.870-0.909 | 0.899-0.935 |

The meshes and vertex counts are identical for all variants.

Two more programs, with `-DWITH_ID_MAP` in their harnesses:

- **Redpanda's leader balancer** (`redpanda_lb.sh`): with 80 groups, 4.60 ms against main's map 5.04
  and 4.5.0's 4.55 (clang); with one ID per raft group (92160), 3.71 ms against 5.54 and 4.81, the
  fastest of the seven containers there, gcc alike.
- **`small_hits`' dense IDs** (#346): 0.51-0.68 of the map's time in a quiet loop and 0.62-0.87 in a
  busy one, at 1000-16000 entries.

## Where the ideas came from

Five implementations were read before writing this one.

| read | taken | left out |
|---|---|---|
| Linux `xarray` | allocate on demand; never copy when growing | the tree and its depth |
| Judy (`JudyL`) | a bitmap with values packed by rank; the layout chosen per node from its count | about 70 node types, 6 levels |
| EnTT (`sparse_set`, `storage`) | pages behind a pointer directory; one shared empty page | a packed dense array that costs 4 dependent loads and loses ID order |
| flecs | values inline in their page | a switch to a hash map when it measures low density (a guess about the caller's data) |
| LLVM (`IndexedMap`, `SparseSet`) | the key as the index | an array sized to the whole key range; an in-band null value |

## Disadvantages

**It is for IDs, not for arbitrary integers.** The directory costs 24 bytes per 4096 IDs up to the
largest ID: 25 MB for a single key near 2^32, and nothing sensible for 64-bit hashes or random keys.
A hash map is the right tool there.

**The memory lead depends on the API.** Handing out a `std::pair<K, T>&` from iterators, as standard
maps do, needs the key stored next to every value: about 50% more memory (dense 12.2 instead of 8.2
bytes per entry, web-ifc-like 21.3 instead of 14.3). With that API the lead over the map shrinks to
0.44-0.93 of its memory. The cheaper layout hands out a proxy `{key, T&}`, which is not a standard
container interface.

**Erase-heavy use costs memory.** With IDs that drift upward (erase a random live ID, insert the next
new one), `id_map` stays fast but holds more memory than the map, because packed pages keep the
capacity they once had and direct pages stay partly filled:

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="img/churn-dark.svg">
  <img alt="Churn with a million live IDs: id_map 19.6 ns and 26.7 bytes per live entry, the map 40.7 ns and 24.1 bytes, std 147 ns and 43.6 bytes, the vector 16.2 ns and 201 bytes." src="img/churn-light.svg">
</picture>

| churn, clang: ns per erase + insert / bytes per live entry | std::unordered_map | unordered_dense::map 5.3.1 | vector indexed by ID | id_map | id_map, std::pair API |
|---|---|---|---|---|---|
| 16,000 | 31.2 / 42.3 | 15.1 / 23.6 | 3.8 / 196.6 | 13.4 / 26.0 | 16.3 / 38.5 |
| 100,000 | 45.2 / 45.8 | 20.7 / 22.9 | 6.5 / 251.7 | 14.9 / 26.7 | 18.2 / 39.2 |
| 1,000,000 | 147.1 / 43.6 | 40.7 / 24.1 | 16.2 / 201.3 | 19.6 / 26.7 | 23.5 / 39.2 |

**Building is slower than a vector** (3.9 against 1.5 ns per dense ID, clang), because a page spends
its first 64 entries packed and then converts. On sparse IDs it builds slower than the map up to
100000 entries (8.4-9.7 against 6.6-7.2 ns per ID, both compilers) and faster at a million (8.6-9.7
against 10.8-11.0): packed pages grow by 1.25x and keep 64 counts per page current.

**Sparse lookups depend on the build flags.** The rank uses a popcount. Built for baseline x86-64,
that is a software popcount of about 14 instructions; with `-mpopcnt` (x86-64-v2 or `-march=native`)
sparse lookups take 0.72-0.76 of the time. A header cannot turn it on by itself. Dense pages never
compute a rank and do not care.

**The per-page switch decides from the data.** Each page chooses its layout from its own entry count.
It guesses nothing about IDs not yet inserted and never changes a result, but it is a decision taken
from the caller's data, and this repository has declined optimizations that infer the caller's input
(#248). Whether this one is within that rule is open.

**Not measured**: values larger than 16 bytes, MSVC, 32-bit builds, a batched or prefetching lookup
(the map's `visit` gained 1.18-1.29x past the cache that way), huge pages.

## Tried and rejected

Three changes looked promising and did not help (`idbench tune`, two passes per compiler):

- **Bitmap and slots in one allocation**: no faster; under clang 5.5 to 5.8-5.9 ns at 3.5M dense IDs.
  The gap it was meant to close was run-to-run variation.
- **Direct pages growing 4x instead of 2x**: builds 8-10% faster, in-cache lookups 6-10% slower under
  clang. Lookups matter more.
- **Back to packed at 1/4 instead of 1/8**: more memory under churn (31 against 26.7 bytes per live
  entry) and 1.5-2x the time, because packed pages keep their capacity and inserts into them shift.

## Reproducing

| what | how |
|---|---|
| correctness | `clang++ -std=c++17 -g -O1 -fsanitize=address,undefined check.cpp && ./a.out` |
| microbenchmark | `clang++ -O3 -DNDEBUG -std=c++17 -I../../../include idbench.cpp && taskset -c 2 ./a.out` (`dense`, `ifc`, `sparse`, `churn`, `tune`) |
| web-ifc | `../web-ifc/build.sh`, then `../web-ifc/run.sh` (see `../web-ifc/README.md`) |
| Redpanda, small_hits | `AB_DEFINES=-DWITH_ID_MAP ../redpanda_lb.sh`, `-DWITH_ID_MAP` on `../small_hits.cpp` |
| these figures | `python3 plots.py`, from the result files in `data/` |

The full record, with every measurement and its conditions, is the notes entry "An `id_map` for
integer IDs the caller knows are nearly dense (#379)" in `notes/index-design.md`.
