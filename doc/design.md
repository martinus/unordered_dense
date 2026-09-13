# Design

[README](../README.md) · [Usage](usage.md) · **Design** · [Benchmarks](benchmarks.md) · [Real world usage](users.md)

How `ankerl::unordered_dense::map` and `set` store their elements and find them again. The API is
in [Usage](usage.md), the numbers in [Benchmarks](benchmarks.md).

The map/set has two data structures:
* `std::vector<value_type>` which holds all data. map/set iterators are just `std::vector<value_type>::iterator`!
* An indexing structure, which is a flat array of blocks. Each block is one group of sixteen slots: their fingerprints, the group's overflow counters, and the sixteen value indices, all in the same 88 bytes.

## Inserts

Whenever an element is added, it is `emplace_back`ed to the vector. The key is hashed, and the index
records where the value went. The index is groups of sixteen slots:

```cpp
struct block {
    uint8_t  m_fingerprints[16]; // the low byte of the hash, 0 means empty
    uint8_t  m_overflows[8];     // how many entries with (fingerprint & 7) == i probed past this group
    uint32_t m_index[16];        // where in the value vector each occupied slot's element is
};                               // 88 bytes, one per group, in a single array
```

The top bits of the hash pick the group, the low byte is the fingerprint, with 0 mapped to 8 so
that 0 can mean "empty" and the low three bits, which select one of the eight counters, are
unchanged. An insert takes the first free slot from the home group onwards, in a quadratic
sequence over groups, and increments its counter in every full group it passed.

## Lookups

The key is hashed, the group's sixteen fingerprints are loaded at once and compared against the
key's fingerprint in one instruction, and the result is a 16 bit mask of candidate slots. For each
candidate the value index is read and the key in the data vector is compared; when equal, the value
is returned. If no candidate matched, the one overflow counter that the fingerprint selects decides:
zero means no entry with those bits ever left this group, so the key is absent, and otherwise the
probe moves to the next group. It also gives up once it has visited every group, which is as far as
any key that exists can have been placed. That bound matters: a counter counts the entries that
overflowed past its group on *their* probe sequences, so with a hash the caller controls every
group's counter can be left positive by a handful of keys, and a miss would then have nothing on
its sequence to stop at.

A slot's value index sits at a fixed offset from the fingerprints it belongs to rather than in a
second array at a second address, so a lookup touches one region instead of two and the line the
index is on is prefetched while the fingerprints are still on their way.

An element that arrived while its home group was full sits in a later group, and it stays there
even after the home group empties again. So a long-churned table probes a little further than one
built from the same contents, and not by much: at a load of 0.76, after 200 full turnovers, 1.036
groups per hit against a fresh 1.031, and 1.061 per miss against 1.052. At a load of 0.79 it is
1.058 against 1.037 and 1.109 against 1.077. It settles there rather than growing, which is the
difference from a design that leaves tombstones behind and has to rehash them away.

A lookup that *finds* something inside an operation that writes -- `operator[]`, `try_emplace`,
`insert` -- puts that element back in its home group if there is room, which costs a load and two
stores and needs no second hash, since the probe just computed the home. That takes the drift back
and then some: one such lookup per erase-and-insert round leaves the churned table at 1.023 groups
per hit and 1.036 per miss, *better* than freshly built, because it also pulls home the elements the
original build left away from home. A workload that only reads gets none of this, and `rehash()`
rebuilds the index if you want the difference back that way.

Without a vector compare the same sixteen bytes are compared eight at a time with ordinary
arithmetic, see [Disabling the Vector Probe](usage.md#disabling-the-vector-probe).

## Removals

Since all data is stored in a vector, removals are a bit more complicated:

1. First, look up the element to delete in the index.
2. Clear its fingerprint, and decrement the overflow counter in every group between its home group
   and the one it landed in. An erase undoes exactly what the insert did, so there are no
   tombstones and no rehash is ever needed to repair the index. Nothing else moves.
3. Replace that element in the vector with the last element in the vector.
4. Update the slot of the moved element, which requires another lookup.
