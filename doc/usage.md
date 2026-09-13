# Usage

[README](../README.md) · **Usage** · [Design](design.md) · [Benchmarks](benchmarks.md) · [Real world usage](users.md)

Almost everything `std::unordered_map` and `std::unordered_set` have is here, and works the same
way. This page is about the rest: the hash, the API a vector of values makes possible, and the
shapes `ankerl::unordered_dense::map` and `set` can be asked to take. The index itself is in
[Design](design.md).

## Modules

`ankerl::unordered_dense` supports c++20 modules. Simply compile `src/ankerl.unordered_dense.cpp` and use the resulting module, e.g. like so:

```sh
clang++ -std=c++20 -I include --precompile -x c++-module src/ankerl.unordered_dense.cpp
clang++ -std=c++20 -c ankerl.unordered_dense.pcm
```

To use the module, e.g. in `module_test.cpp`, use 

```cpp
import ankerl.unordered_dense;
```

and compile with e.g.

```sh
clang++ -std=c++20 -fprebuilt-module-path=. ankerl.unordered_dense.o module_test.cpp -o main
```

A simple demo script can be found in `test/modules`.

The module compares fingerprints without SSE2, see [Disabling the Vector Probe](#disabling-the-vector-probe). If you
wrap the header in a module of your own and build it with gcc, you need to do the same.

## Hash

`ankerl::unordered_dense::hash` is a fast and high quality hash, based on [wyhash](https://github.com/wangyi-fudan/wyhash). The `ankerl::unordered_dense` map/set differentiates between high quality hashes (good [avalanching effect](https://en.wikipedia.org/wiki/Avalanche_effect)) and low quality hashes. High quality hashes contain a special marker:

```cpp
using is_avalanching = void;
```

This is the case for the specializations `bool`, `char`, `signed char`, `unsigned char`, `char8_t`, `char16_t`, `char32_t`, `wchar_t`, `short`, `unsigned short`, `int`, `unsigned int`, `long`, `long long`, `unsigned long`, `unsigned long long`, `T*`, `std::unique_ptr<T>`, `std::shared_ptr<T>`, `enum`, `std::basic_string<C>`, and `std::basic_string_view<C>`.

Hashes that do not contain this marker are assumed to be of low quality and receive an additional mixing step inside the map/set implementation. The marker can also be spelled `using is_avalanching = std::true_type;`, and given for a hash you cannot edit -- see [Marking a Hash Avalanching From Outside](#marking-a-hash-avalanching-from-outside).

### Simple Hash

Consider a simple custom key type:

```cpp
struct id {
    uint64_t value{};

    auto operator==(id const& other) const -> bool {
        return value == other.value;
    }
};
```

The simplest implementation of a hash is this:

```cpp
struct custom_hash_simple {
    auto operator()(id const& x) const noexcept -> uint64_t {
        return x.value;
    }
};
```
This can be used, e.g. with 

```cpp
auto ids = ankerl::unordered_dense::set<id, custom_hash_simple>();
```

Since `custom_hash_simple` doesn't have a `using is_avalanching = void;` marker, it is considered to be of low quality and additional mixing of `x.value` is automatically provided inside the set.

### High Quality Hash

Back to the `id` example, we can easily implement a higher quality hash:

```cpp
struct custom_hash_avalanching {
    using is_avalanching = void;

    auto operator()(id const& x) const noexcept -> uint64_t {
        return ankerl::unordered_dense::detail::wyhash::hash(x.value);
    }
};
```

We know `wyhash::hash` is of high quality, so we can add `using is_avalanching = void;` which makes the map/set directly use the returned value.

### Specialize `ankerl::unordered_dense::hash`

Instead of creating a new class you can also specialize `ankerl::unordered_dense::hash`:

```cpp
template <>
struct ankerl::unordered_dense::hash<id> {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(id const& x) const noexcept -> uint64_t {
        return detail::wyhash::hash(x.value);
    }
};
```

### Heterogeneous Overloads using `is_transparent`

This map/set supports heterogeneous overloads as described in [P2363 Extending associative containers with the remaining heterogeneous overloads](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2363r3.html) which is [targeted for C++26](https://wg21.link/p2077r2). This has overloads for `find`, `count`, `contains`, `equal_range` (see [P0919R3](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0919r3.html)), `erase` (see [P2077R2](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2077r2.html)), and  `try_emplace`, `insert_or_assign`, `operator[]`, `at`, and `insert` & `emplace` for sets (see [P2363R3](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2363r3.html)).

For heterogeneous overloads to take effect, both `hasher` and `key_equal` need to have the attribute `is_transparent` set.

Here is an example implementation that's usable with any string type that is convertible to `std::string_view` (e.g. `char const*` and `std::string`):

```cpp
struct string_hash {
    using is_transparent = void; // enable heterogeneous overloads
    using is_avalanching = void; // mark class as high quality avalanching hash

    [[nodiscard]] auto operator()(std::string_view str) const noexcept -> uint64_t {
        return ankerl::unordered_dense::hash<std::string_view>{}(str);
    }
};
```

To make use of this hash you'll need to specify it as a type, and also a `key_equal` with `is_transparent` like [std::equal_to<>](https://en.cppreference.com/w/cpp/utility/functional/equal_to_void):

```cpp
auto map = ankerl::unordered_dense::map<std::string, size_t, string_hash, std::equal_to<>>();
```

For more information see the examples in `test/unit/transparent.cpp`.

### Automatic Fallback to `std::hash`

When an implementation for `std::hash` of a custom type is available, it is automatically used and assumed to be of low quality (thus `std::hash` is used, but an additional mixing step is performed).

If your `std::hash` specialization is a high quality one, say so there and it is taken at its word -- the extra mixing is then skipped, exactly as for a hash written in `ankerl::unordered_dense`. The fallback asks `hash_is_avalanching` like everything else, so either spelling of the marker works, and a `std::hash` you cannot edit can be named from outside ([Marking a Hash Avalanching From Outside](#marking-a-hash-avalanching-from-outside)):

```cpp
template <>
struct std::hash<id> {
    using is_avalanching = void;

    auto operator()(id const& x) const noexcept -> size_t {
        return ankerl::unordered_dense::detail::wyhash::hash(x.value);
    }
};
```

### Hash the Whole Memory

When the type [has a unique object representation](https://en.cppreference.com/w/cpp/types/has_unique_object_representations) (no padding, trivially copyable), one can just hash the object's memory. Consider a simple class

```cpp
struct point {
    int x{};
    int y{};

    auto operator==(point const& other) const -> bool {
        return x == other.x && y == other.y;
    }
};
```

A fast and high quality hash can be easily provided like so:

```cpp
struct custom_hash_unique_object_representation {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(point const& f) const noexcept -> uint64_t {
        static_assert(std::has_unique_object_representations_v<point>);
        return ankerl::unordered_dense::detail::wyhash::hash(&f, sizeof(f));
    }
};
```

### Marking a Hash Avalanching From Outside

`using is_avalanching = void;` is a member of the hash, which is no help when the hash comes from a library you cannot edit. `hash_is_avalanching` is what the map and set actually ask, and it can be answered from outside:

```cpp
template <>
struct ankerl::unordered_dense::hash_is_avalanching<their::good_hash> : std::true_type {};
```

The extra mixing is now skipped for `their::good_hash` everywhere, without touching it. The specialization also works the other way -- `std::false_type` makes the map mix a hash's output whatever the hash claims about itself, which is the escape hatch for one that promises more than it delivers.

This is deliberately the same name, the same two ways of answering, and the same meaning as [`boost::hash_is_avalanching`](https://www.boost.org/doc/libs/latest/libs/unordered/doc/html/unordered/reference/hash_traits.html), so a hash annotated for Boost.Unordered is read correctly here and the other way around. The member may therefore also be written as a compile time bool, which is the spelling Boost's documentation asks for:

```cpp
using is_avalanching = std::true_type;   // same as `= void`
using is_avalanching = std::false_type;  // says the opposite
```

Boost calls `= void` deprecated; here it stays the ordinary spelling, since it is what this library has always documented and what every hash in the header uses. Writing anything else there -- a stray `int`, say -- is a compile error rather than a silent yes or no.

### Requiring an Avalanching Hash

In a codebase where every hash is meant to be a high quality one, forgetting to say so is the easy mistake, and nothing complains -- the map just quietly mixes. Wrap the hash to make it a build error instead:

```cpp
template <class Key, class T>
using my_map = ankerl::unordered_dense::map<Key, T, ankerl::unordered_dense::require_avalanching<my_hash<Key>>>;
```

The requirement is written into the alias rather than next to the hash, so it is part of what `my_map` *is*, and survives `my_hash` being reimplemented without its marker -- which a `static_assert` next to the hash does not. (It does not follow a map that is given a different hash outright: `map<K, V, other_hash>` names no requirement, so there is none.) It accepts a hash marked either way, by its own member typedef or by a `hash_is_avalanching` specialization.

The hash must not be `final`, since the wrapper derives from it -- for one that is, specialize `hash_is_avalanching` instead. A stateful hash goes in either braced or by value: `require_avalanching<my_hash>{my_hash{seed}}`.

## Container API

In addition to the standard `std::unordered_map` API (see https://en.cppreference.com/w/cpp/container/unordered_map), we have additional API that is somewhat similar to the node API, but builds on the fact that the values live in a random access container:

### `auto replace_key(iterator it, K&& new_key) -> std::pair<iterator, bool>`

Updates the key of an element in-place without changing its position in the underlying container. This operation maintains iterator and reference stability - all existing iterators and references remain valid after the update.

Note that this can also be used as an optimization for `unordered_dense::set` when you want to `erase` one element and then `insert` a new element, this should be quite a bit faster.

### `auto extract() && -> value_container_type`

Extracts the internally used container. `*this` is emptied.

### `extract()` Single Elements

Similar to `erase()`, there is an API call `extract()`. It behaves exactly the same as `erase`, except that the return value is the moved element that is removed from the container:

* `auto extract(const_iterator it) -> value_type`
* `auto extract(Key const& key) -> std::optional<value_type>`
* `template <class K> auto extract(K&& key) -> std::optional<value_type>`

Note that the `extract(key)` API returns an `std::optional<value_type>` that is empty when the key is not found.

### `[[nodiscard]] auto values() const noexcept -> value_container_type const&`

Exposes the underlying values container.

### `auto replace(value_container_type&& container)`

Discards the internally held container and replaces it with the one passed. Non-unique elements are
removed, and the container will be partly reordered when non-unique elements are found.

### `auto hash_for(K const& key) const -> precomputed_hash`

Hashing a key is usually the largest part of a lookup, and looking up the same key over and over hashes it every time. `hash_for()` does it once, and `find`, `contains`, `count`, `equal_range` and `at` each take what it returns as a second argument:

```cpp
auto map = ankerl::unordered_dense::map<std::string, int>();
// ...

// hash it once, e.g. at startup
auto const status_hash = map.hash_for("status");

// as often as you like
auto it = map.find("status", status_hash);
```

The key is still needed -- a lookup that lands on a bucket still has to compare keys to know it found the right one. What is skipped is the hashing, so the longer the key the more there is to gain (clang 18, x86-64, half hits and half misses):

| key length | `find(key)` | `find(key, hash)` | |
| ---------: | ----------: | ----------------: | ---: |
| 8 bytes | 5.6 ns | 4.0 ns | 1.4x |
| 32 bytes | 7.1 ns | 4.3 ns | 1.7x |
| 200 bytes | 22.5 ns | 7.4 ns | 3.0x |

`precomputed_hash` is a distinct type rather than a plain integer, because the number a lookup wants is *not* what `hash_function()` returns -- the table finalizes that further -- and an integer parameter would happily accept the wrong one. An integer does not convert to it; the value inside stays reachable, so a hash can be stored or moved around freely.

A hash belongs to the hasher, not to the table it came from. It stays valid across insertions, erasures, `rehash()` and moves, and every table using the same hasher takes it -- so one hash can serve a map and a set together:

```cpp
auto set = ankerl::unordered_dense::set<std::string>();
auto found = set.find("status", status_hash); // the hash from the map above
```

What it does not survive is the key changing. Looking up a key with the hash of a different key does not throw or crash -- it just quietly reports the key as not present.

Heterogeneous lookup works as usual when the hash and equality are transparent, and the hash may be taken from one key type and used with another:

```cpp
auto const h = map.hash_for(std::string_view("status"));
auto it = map.find("status"s, h);
```

Only lookups take a precomputed hash, and insertion never will: a lookup given the wrong hash merely misses, while an insertion given one files the element under a probe chain it is not on, losing it for good and letting a second copy of the same key in beside it. Erase is left out for a duller reason -- it hashes the moved element as well as the key, so precomputing the key's hash would save it only half its hashing.

### `auto visit(FwdIt first, FwdIt last, F f) -> size_t`

Looks up a whole range of keys, calls `f` on each one that is there, and returns how many that was.

```cpp
auto const keys = std::vector<std::string>{"alpha", "beta", "gamma"};
auto total = 0;
auto found = map.visit(keys.begin(), keys.end(), [&](auto const& kv) { total += kv.second; });
```

`f` receives `value_type&`, or `value_type const&` on a `const` map, so a visit can modify what it finds. Keys that are absent are not reported; the count says how many were there.

**Why it is faster than the same loop of `find()`.** A lookup on a table past the cache is two dependent memory accesses -- the group's block, and then the value the slot points at -- and a loop doing one lookup at a time can only overlap them as far as the processor's own reordering reaches past a whole loop body. `visit` works a chunk at a time in three passes: every key's block is asked for, then the fingerprints are matched once the blocks have arrived, then the keys are compared. Every block in the chunk is in flight at once.

`map<uint64_t, size_t>`, clang 22 on a 7950X, ns per lookup, against the same batch looked up one key at a time:

| entries | | one at a time | `visit` | |
| ------: | :--- | ----: | ----: | ---: |
| 4 000 000 | all hits | 34.0 | 26.3 | 1.29x |
| 4 000 000 | half hits | 34.4 | 28.4 | 1.21x |
| 16 000 000 | all hits | 36.5 | 30.4 | 1.20x |
| 16 000 000 | half hits | 37.3 | 31.5 | 1.18x |

**It needs a table past the cache to be worth anything**, like every other memory-level trick here: on a map that fits in L2 there is nothing to overlap and the extra passes are a small loss.

**And the batching itself matters more than `visit` does.** If the keys are being fetched from somewhere in the same loop that looks them up -- a random index into another array, say -- then the key's own cache miss sits in front of the map's and neither overlaps with anything. Collecting the keys first and looking them up afterwards is worth **1.5x** at four million entries before `visit` is involved at all:

```cpp
for (size_t i = 0; i < n; ++i) {              // 51 ns per lookup
    auto it = map.find(keys[indices[i]]);
}

std::vector<key_type> batch;                  // 33 ns per lookup
for (size_t i = 0; i < n; ++i) { batch.push_back(keys[indices[i]]); }
for (auto const& k : batch) { auto it = map.find(k); }
```

That is a property of loops and memory parallelism rather than of this map, and it is the larger of the two effects. `visit` is what is left on top once the loop is already shaped that way.

### `void merge(map& source)`

This is the standard container's `merge`, with the guarantees a container without nodes can give. Every element of `source` whose key is not here already moves over, and the rest stay behind. An element whose key is already here is **not** overwritten -- the value already in this map wins, as with `insert` and `try_emplace`. `source` may hash and compare differently; the keys that move are re-hashed with this map's hasher. Both an lvalue and an rvalue `source` are accepted, and an rvalue one is still only emptied of what moved.

```cpp
auto a = ankerl::unordered_dense::map<std::string, int>{{"x", 1}, {"y", 2}};
auto b = ankerl::unordered_dense::map<std::string, int>{{"y", 20}, {"z", 30}};
a.merge(b);
// a is {"x", 1}, {"y", 2}, {"z", 30}   -- "y" was already in a, so a's value stayed
// b is {"y", 20}                       -- and so did b's
```

Two things differ from `std::unordered_map::merge`, and both follow from the elements living in a vector rather than in nodes:

* **Iterators and references into either container are invalidated.** A node-based merge splices nodes, so references to the elements that move stay valid. Here an element that moves is move-constructed into the destination's vector, which may reallocate.
* **The source's order changes.** Every element taken out of the middle of it leaves a gap that the elements behind it close. `erase()` already reorders for the same reason.

`a.merge(a)` has no effect. If an operation throws -- a hash, a key comparison, or an element's move -- both containers are left valid and usable, `source` keeps everything not yet taken, and the one element that was being moved at the time may be lost.

`merge` is worth using over the loop it replaces: about **2x** when the two maps mostly do not overlap, which is what a merge is usually for.

## Custom Container Types

`unordered_dense` accepts a custom allocator, but you can also specify a custom container for that template argument. That way it is possible to replace the internally used `std::vector` with e.g. `std::deque` or any other container like `boost::interprocess::vector`. This supports fancy pointers (e.g. [offset_ptr](https://www.boost.org/doc/libs/1_80_0/doc/html/interprocess/offset_ptr.html)), so the container can be used with e.g. shared memory provided by `boost::interprocess`.

## `segmented_map` and `segmented_set`

`ankerl::unordered_dense` provides a custom container implementation that has lower memory requirements than the default `std::vector`. Memory is not contiguous, but it can allocate segments without having to reallocate and move all the elements. In summary, this leads to

* Much smoother memory usage of the values, which increases continuously.
* No high peak memory usage from the values.
* Faster insertion because elements never need to be moved to newly allocated blocks
* Slightly slower indexing compared to `std::vector` because an additional indirection is needed.

Here is what each of four maps holds while 10 million `uint64_t -> uint64_t` pairs are inserted into it:
![allocated memory](allocated_memory.png)

| inserting 10M pairs | held at the end | peak while filling |
|---|---|---|
| `ankerl::unordered_dense::map` | 361 MB | 495 MB |
| `ankerl::unordered_dense::segmented_map` | **253 MB** | **253 MB** |
| `boost::unordered_flat_map` | 268 MB | 403 MB |
| `absl::flat_hash_map` | 285 MB | 428 MB |

Every flat and dense map in that chart has the same sawtooth, and for the same reason: growing means allocating the new array before releasing the old one, so the transient is what a caller has to have room for even though nothing ever reports it. `ankerl::unordered_dense::map` has the tallest one, because a dense map grows a vector of values as well as an index.

`segmented_map` is the line without a sawtooth. Its values live in fixed-size segments, so growing adds a segment instead of copying everything into a bigger block, and the memory it holds only ever goes up. The one step still visible in that line is the index doubling, which segmenting does not remove -- but it happens while the values are still small, so on this run it never rises above where the map ends up, and the peak and the steady state are the same number.

The segmenting is about the values: it is those that grow smoothly and whose references stay valid. The index is one plain contiguous array either way, and growing it still allocates the new one beside the old. Since 5.0.0 that is a change from before, when the index was segmented too.

Each line runs to the end of its own fill and then drops to zero, which is that map being destroyed -- for the dense maps in two steps, the index and then the values. So where a line falls off is how long that map took to fill: 0.41 s for boost, 0.47 s for abseil, 0.50 s for `segmented_map` and 0.57 s for `map` on this machine. Do not read that as a build benchmark, though. This chart deliberately does not raise glibc's mmap threshold the way the benchmark suite does, so every large block here is faulted in from the kernel a page at a time, and it is measuring memory rather than speed.

The chart is drawn by `scripts/ab/alloc_timeline.sh`, which counts *every* allocation the process makes by replacing global `operator new` -- an allocator handed to a container sees only what that container asks for through it -- charges each one what the allocator really gave away (`malloc_usable_size` plus glibc's chunk header, so the rounding up is counted rather than guessed at), and takes a `std::chrono::steady_clock` reading at each change. The runtimes on the x axis are from one machine and one run; the byte counts are exact.

How much the remaining index spike matters depends on the size of your value. The index is 5.5 bytes per slot, so at the moment it doubles it needs about 16.5 bytes per slot transiently, against `sizeof(value_type)` bytes per element for the values. For `map<uint64_t, uint64_t>` that spike is roughly two thirds of the value storage; for a map with a large value it is a rounding error; for a `set<uint64_t>` it is larger than the values. If you need the index to grow smoothly as well, `reserve()` up front avoids the doubling entirely, which is worth doing for a large map whatever container it uses.

The size of a segment is a template parameter, and it defaults to 4096 bytes, which is small. A map
that is going on huge pages wants it set: see [Sizing a segment for a huge page](#sizing-a-segment-for-a-huge-page).

## Custom Bucket Types

The index is groups of sixteen slots; the bucket type chooses how wide a value index is. The
default should be good for pretty much everyone. See [the design notes](design.md) for how the index
works.

### `ankerl::unordered_dense::bucket_type::group`

* Up to 2^32 = 4.29 billion elements.
* 5.5 bytes overhead per slot: one 88 byte block per group of sixteen slots, holding the sixteen fingerprints, the group's eight overflow counters and sixteen 4 byte value indices.

### `ankerl::unordered_dense::bucket_type::group_big`

* Up to 2^63 = 9,223,372,036,854,775,808 elements.
* 9.5 bytes overhead per slot: the same block with 8 byte value indices instead of 4 byte ones, so 152 bytes per group.

## Disabling the Vector Probe

A probe compares a group's sixteen fingerprints at once: with one SSE2 instruction on x86-64, and
with NEON on AArch64. Neither needs a compiler flag, because both are part of their target's
baseline. Defining `ANKERL_UNORDERED_DENSE_HAS_SSE2` or `ANKERL_UNORDERED_DENSE_HAS_NEON` to 0
before the header is included switches to the portable fallback, which compares eight fingerprints
per machine word with ordinary arithmetic, e.g. with cmake:

```cmake
target_compile_definitions(your_target PRIVATE ANKERL_UNORDERED_DENSE_HAS_SSE2=0)
```

The module in `src/` sets both already, because the intrinsics are declared by headers included in
the global module fragment and those declarations do not reach a translation unit that imports the
module. Wrapping the header in a module of your own means doing the same.

All three compare the same sixteen bytes and differ only in how they report which lanes matched, so
translation units that disagree about these macros still agree about every byte of the index they
share. On x86-64 the word-at-a-time fallback is within a few percent of SSE2 on the benchmark's
workloads; on AArch64 it is not, which is why the NEON path exists.

## LLDB Data Formatters

The repository ships a formatter script for LLDB in [`lldb/unordered_dense.py`](../lldb/unordered_dense.py). It makes
`map`, `set`, `segmented_map`, `segmented_set` (including the `pmr::` variants) and `segmented_vector` print like
regular containers instead of raw internals, across `map`'s every flavor with one provider:

```
(lldb) frame variable word_count
(ankerl::unordered_dense::map<std::string, int> &) word_count = size=3 bucket_count=64 {
  [alpha] = (first = "alpha", second = 1)
  [beta] = (first = "beta", second = 2)
  [gamma] = (first = "gamma", second = 3)
}
```

Load it with

```
command script import /path/to/unordered_dense/lldb/unordered_dense.py
```

or put that line into `~/.lldbinit` to always have it. Elements are the densely stored values in the container's
iteration order -- insertion order until something is erased -- and children are named after their key when the key
renders as a short scalar or string (`v map[2]` works by index regardless). The script only reads memory, so it is
safe on core dumps, and `frame variable -R <var>` still shows the raw members whenever they are wanted. Naming
children by key can be turned off with

```
script unordered_dense.NAME_CHILDREN_BY_KEY = False
```

Custom value containers (see [Custom Container Types](#custom-container-types)) fall back to whatever LLDB itself can display for
them.

## Huge Pages

A lookup touches two or three random addresses -- the group's block, the value it points at, and for a string key its body -- and on 4 KB pages each of them is an address translation. A first-level data TLB holds on the order of 64-96 entries, a few hundred KB, so any table larger than that pays an L2 TLB lookup per access, and on a dependent chain that lookup is latency. Measured on this library's own scored benchmark by running the same binary with its heap on 2 MB pages: **2.6% (gcc) to 3.6% (clang) over the whole score, 5-8% on churn and on random finds at 50000 entries, and 22% of a lookup past the last-level cache**. Nothing asks for huge pages by default, and on the common Linux setting (`/sys/kernel/mm/transparent_hugepage/enabled` = `madvise`) nothing gets them without asking. There are two ways to ask.

**The environment, for the whole process.** glibc 2.35 and later can `madvise` its heap for you:

```sh
GLIBC_TUNABLES=glibc.malloc.hugetlb=1 ./your_program
```

This is what the numbers above were measured with, and it is the route that helps *small* tables too, because neighbouring blocks share a 2 MB extent on the heap. Setting the THP mode to `always` does the same for every process on the machine.

**The allocator, for one map.** `include/ankerl/huge_page_allocator.h` is a separate header (it needs `<sys/mman.h>`) with an allocator that puts every block of 2 MB and up on its own 2 MB-aligned, `MADV_HUGEPAGE`d mapping and leaves smaller ones to `std::allocator`:

```cpp
#include <ankerl/huge_page_allocator.h>

using map_t = ankerl::unordered_dense::huge_page::map<uint64_t, uint64_t>;
```

`huge_page::map`, `segmented_map`, `set` and `segmented_set` fill the allocator slot for you. This names the same type:

```cpp
using map_t = ankerl::unordered_dense::map<uint64_t, uint64_t,
                                           ankerl::unordered_dense::hash<uint64_t>,
                                           std::equal_to<uint64_t>,
                                           ankerl::unordered_dense::huge_page_allocator<std::pair<uint64_t, uint64_t>>>;
```

Both the index and the values get it, since the index rebinds the value allocator. What it is worth, ns per operation with `std::allocator` and with this one, on the scored workloads at sizes the score does not run (clang, Ryzen 9 7950X; gcc within a few percent of the same ratios):

| | 200000 | 800000 | 4000000 |
|---|---|---|---|
| build from empty, `uint64_t` | 20.4 → 13.3 (**1.53x**) | 22.1 → 12.8 (**1.73x**) | 38.4 → 24.6 (**1.56x**) |
| build from empty, `std::string` | 71.9 → 63.6 (1.13x) | 85.5 → 68.6 (1.25x) | 121.8 → 94.4 (1.29x) |
| churn at a fixed size, `uint64_t` | 12.7 → 12.3 (1.03x) | 16.1 → 12.1 (**1.33x**) | 53.7 → 49.4 (1.09x) |
| random find, 50% hits, `uint64_t` | 5.49 → 5.45 | 6.50 → 6.21 (1.05x) | 16.3 → 14.3 (1.14x) |

A build gains more than the TLB explains: a vector that doubles faults in every new block, and on 2 MB pages that is 512 times fewer faults. It is not specific to this map -- `boost::unordered_flat_map` on the same allocator gains 1.5-1.9x on integer builds and 1.62x on churning 64 byte values, where its single region holds the values inline -- so it changes nothing about which map is ahead where; the full table is in `notes/index-design.md`. Two things to know:

* **It only helps once the blocks themselves reach 2 MB**, which is the index from about 370000 entries and the values from `2 MB / sizeof(value_type)` entries. A huge page is 2 MB whole, and an allocator that owns only its own blocks has no neighbour to share one with -- that is what the environment route has and this one does not. Below that size, use the environment.
* **Every block is rounded up to 2 MB, and the rounding is resident memory**, because touching one byte of an `MADV_HUGEPAGE`d extent populates all of it. The map doubles both regions, so the loss is at most half a doubling step per region, while that region sits between doublings. The threshold is the second template parameter (`huge_page_allocator<T, 4 << 20>`), and it cannot go below 2 MB; the `huge_page::` aliases use the default, so a different threshold is the allocator written out.

### Sizing a segment for a huge page

A segmented map never reallocates its values, so a segment on a huge page has no rounding loss to amortize and no copy to pay. `segmented_map`, `segmented_set` and their `pmr::` and `huge_page::` twins take the segment size as a trailing parameter, after the bucket. It defaults to `default_segment_size_bytes`, which is 4096 -- far below the allocator's threshold, so a segmented map wants it set:

```cpp
ankerl::unordered_dense::huge_page::segmented_map<K, V, ankerl::unordered_dense::hash<K>, std::equal_to<K>,
                                                  ankerl::unordered_dense::bucket_type::group, 16 << 20>
```

A segment holds a power of two of elements, rounded *down* to fit the byte size, so a 2 MB segment is exactly one huge page for a 16 byte pair and 1.28 MB -- below the threshold, no huge page -- for a 40 byte one. 16 MB segments bound that rounding at 2 MB each for any element size, and are the setting to use unless `sizeof(value_type)` is a power of two. Measured at 800000 entries, ns per operation, `std::vector` on `std::allocator` / segmented on this allocator with 16 MB segments: build `uint64_t` 22.0 / 14.7, `std::string` 85.6 / **63.3**, 64 byte values 77.1 / **20.5**; churn `uint64_t` 16.1 / 14.5, `std::string` 101.0 / 95.6. The segmented container costs 10-20% on lookups and churn for its extra indirection, and huge pages do not take that back; on builds it is the fastest thing here, because it neither copies nor faults.

On Windows and macOS the class exists with the same interface and forwards everything to `std::allocator`; `huge_page_allocator<T>::uses_huge_pages` says which you got. `scripts/ab/huge_pages.sh` measures it across sizes and workloads, and the measurements are in `notes/index-design.md`.
