<a id="top"></a>

[![Release](https://img.shields.io/github/release/martinus/unordered_dense.svg)](https://github.com/martinus/unordered_dense/releases)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/martinus/unordered_dense/main/LICENSE)
[![meson_build_test](https://github.com/martinus/unordered_dense/actions/workflows/main.yml/badge.svg)](https://github.com/martinus/unordered_dense/actions)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/6220/badge)](https://bestpractices.coreinfrastructure.org/projects/6220)
[![Sponsors](https://img.shields.io/github/sponsors/martinus?style=social)](https://github.com/sponsors/martinus)

# 🚀 ankerl::unordered_dense::{map, set} <!-- omit in toc -->

A fast & densely stored hashmap and hashset for C++17 and later.

The classes `ankerl::unordered_dense::map` and `ankerl::unordered_dense::set` are (almost) drop-in replacements of `std::unordered_map` and `std::unordered_set`. While they don't have as strong iterator / reference stability guarantees, they are typically *much* faster.

Additionally, there are `ankerl::unordered_dense::segmented_map` and `ankerl::unordered_dense::segmented_set` with lower peak memory usage, and stable references (iterators are NOT stable) on insert.

- [1. Overview](#1-overview)
- [2. Installation](#2-installation)
  - [2.1. Installing using cmake](#21-installing-using-cmake)
- [3. Usage](#3-usage)
  - [3.1. Modules](#31-modules)
  - [3.2. Hash](#32-hash)
    - [3.2.1. Simple Hash](#321-simple-hash)
    - [3.2.2. High Quality Hash](#322-high-quality-hash)
    - [3.2.3. Specialize `ankerl::unordered_dense::hash`](#323-specialize-ankerlunordered_densehash)
    - [3.2.4. Heterogeneous Overloads using `is_transparent`](#324-heterogeneous-overloads-using-is_transparent)
    - [3.2.5. Automatic Fallback to `std::hash`](#325-automatic-fallback-to-stdhash)
    - [3.2.6. Hash the Whole Memory](#326-hash-the-whole-memory)
    - [3.2.7. Marking a Hash Avalanching From Outside](#327-marking-a-hash-avalanching-from-outside)
    - [3.2.8. Requiring an Avalanching Hash](#328-requiring-an-avalanching-hash)
  - [3.3. Container API](#33-container-api)
    - [3.3.1. `auto replace_key(iterator it, K&& new_key) -> std::pair<iterator, bool>`](#331-auto-replace_keyiterator-it-k-new_key---stdpairiterator-bool)
    - [3.3.2. `auto extract() && -> value_container_type`](#332-auto-extract----value_container_type)
    - [3.3.3. `extract()` Single Elements](#333-extract-single-elements)
    - [3.3.4. `[[nodiscard]] auto values() const noexcept -> value_container_type const&`](#334-nodiscard-auto-values-const-noexcept---value_container_type-const)
    - [3.3.5. `auto replace(value_container_type&& container)`](#335-auto-replacevalue_container_type-container)
    - [3.3.6. `auto hash_for(K const& key) const -> precomputed_hash`](#336-auto-hash_fork-const-key-const---precomputed_hash)
    - [3.3.7. `auto visit(FwdIt first, FwdIt last, F f) -> size_t`](#337-auto-visitfwdit-first-fwdit-last-f-f---size_t)
    - [3.3.8. `void merge(map& source)`](#338-void-mergemap-source)
  - [3.4. Custom Container Types](#34-custom-container-types)
  - [3.5. Custom Bucket Types](#35-custom-bucket-types)
    - [3.5.1. `ankerl::unordered_dense::bucket_type::group`](#351-ankerlunordered_densebucket_typegroup)
    - [3.5.2. `ankerl::unordered_dense::bucket_type::group_big`](#352-ankerlunordered_densebucket_typegroup_big)
  - [3.6. Disabling the Vector Probe](#36-disabling-the-vector-probe)
  - [3.7. LLDB Data Formatters](#37-lldb-data-formatters)
  - [3.8. Huge Pages](#38-huge-pages)
- [4. `segmented_map` and `segmented_set`](#4-segmented_map-and-segmented_set)
- [5. Design](#5-design)
  - [5.1. Inserts](#51-inserts)
  - [5.2. Lookups](#52-lookups)
  - [5.3. Removals](#53-removals)
- [6. Real World Usage](#6-real-world-usage)
  - [6.1. Databases and data engines](#61-databases-and-data-engines)
  - [6.2. Games, emulators and game engines](#62-games-emulators-and-game-engines)
  - [6.3. Graphics, rendering and GPU compute](#63-graphics-rendering-and-gpu-compute)
  - [6.4. Maps and geospatial](#64-maps-and-geospatial)
  - [6.5. CAD, 3D printing and simulation](#65-cad-3d-printing-and-simulation)
  - [6.6. Bioinformatics](#66-bioinformatics)
  - [6.7. Networking, media and security](#67-networking-media-and-security)
  - [6.8. Finance and blockchain](#68-finance-and-blockchain)
  - [6.9. Tools, libraries and machine learning](#69-tools-libraries-and-machine-learning)
  - [6.10. Ports](#610-ports)

## 1. Overview

The chosen design has a few advantages over `std::unordered_map`: 

* Perfect iteration speed - Data is stored in a `std::vector`, all data is contiguous!
* Very fast insertion & lookup speed, in the same ballpark as [`absl::flat_hash_map`](https://abseil.io/docs/cpp/guides/container)
* Low memory usage
* Full support for `std::allocators`, and [polymorphic allocators](https://en.cppreference.com/w/cpp/memory/polymorphic_allocator). There are `ankerl::unordered_dense::pmr` typedefs available
* Customizable storage type: with a template parameter you can e.g. switch from `std::vector` to `boost::interprocess::vector` or any other compatible random-access container.
* Better debugging: the underlying data can be easily seen in any debugger that can show an `std::vector`.

There's no free lunch, so there are a few disadvantages:

* Deletion speed is relatively slow. This needs two lookups: one for the element to delete, and one for the element that is moved onto the newly empty spot.
* no `const Key` in `std::pair<Key, Value>`
* Iterators and references are not stable on insert or erase.

## 2. Installation

<!-- See https://github.com/bernedom/SI/blob/main/doc/installation-guide.md -->
The default installation location is `/usr/local`.

### 2.1. Installing using cmake 

Clone the repository and run these commands in the cloned folder:

```sh
mkdir build && cd build
cmake ..
cmake --build . --target install
```

Consider setting an install prefix if you do not want to install `unordered_dense` system wide, like so:

```sh
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=${HOME}/unordered_dense_install ..
cmake --build . --target install
```

To make use of the installed library, add this to your project:

```cmake
find_package(unordered_dense CONFIG REQUIRED)
target_link_libraries(your_project_name unordered_dense::unordered_dense)
```

## 3. Usage

### 3.1. Modules

`ankerl::unordered_dense` supports c++20 modules. Simply compile `src/ankerl.unordered_dense.cpp` and use the resulting module, e.g. like so:

```sh
clang++ -std=c++20 -I include --precompile -x c++-module src/ankerl.unordered_dense.cpp
clang++ -std=c++20 -c ankerl.unordered_dense.pcm
```

To use the module, for example in `module_test.cpp`, use 

```cpp
import ankerl.unordered_dense;
```

and compile with e.g.

```sh
clang++ -std=c++20 -fprebuilt-module-path=. ankerl.unordered_dense.o module_test.cpp -o main
```

A simple demo script can be found in `test/modules`.

The module compares fingerprints without SSE2, see [3.6. Disabling the Vector Probe](#36-disabling-the-vector-probe). If you
wrap the header in a module of your own and build it with gcc, you need to do the same.

### 3.2. Hash

`ankerl::unordered_dense::hash` is a fast and high quality hash, based on [wyhash](https://github.com/wangyi-fudan/wyhash). The `ankerl::unordered_dense` map/set differentiates between high quality hashes (good [avalanching effect](https://en.wikipedia.org/wiki/Avalanche_effect)) and low quality hashes. High quality hashes contain a special marker:

```cpp
using is_avalanching = void;
```

This is the case for the specializations `bool`, `char`, `signed char`, `unsigned char`, `char8_t`, `char16_t`, `char32_t`, `wchar_t`, `short`, `unsigned short`, `int`, `unsigned int`, `long`, `long long`, `unsigned long`, `unsigned long long`, `T*`, `std::unique_ptr<T>`, `std::shared_ptr<T>`, `enum`, `std::basic_string<C>`, and `std::basic_string_view<C>`.

Hashes that do not contain this marker are assumed to be of low quality and receive an additional mixing step inside the map/set implementation. The marker can also be spelled `using is_avalanching = std::true_type;`, and given for a hash you cannot edit — see [3.2.7](#327-marking-a-hash-avalanching-from-outside).

#### 3.2.1. Simple Hash

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
This can be used, for example, with 

```cpp
auto ids = ankerl::unordered_dense::set<id, custom_hash_simple>();
```

Since `custom_hash_simple` doesn't have a `using is_avalanching = void;` marker, it is considered to be of low quality and additional mixing of `x.value` is automatically provided inside the set.

#### 3.2.2. High Quality Hash

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


#### 3.2.3. Specialize `ankerl::unordered_dense::hash`

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

#### 3.2.4. Heterogeneous Overloads using `is_transparent`

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


#### 3.2.5. Automatic Fallback to `std::hash`

When an implementation for `std::hash` of a custom type is available, it is automatically used and assumed to be of low quality (thus `std::hash` is used, but an additional mixing step is performed).

If your `std::hash` specialization is a high quality one, say so there and it is taken at its word — the extra mixing is then skipped, exactly as for a hash written in `ankerl::unordered_dense`. The fallback asks `hash_is_avalanching` like everything else, so either spelling of the marker works, and a `std::hash` you cannot edit can be named from outside ([3.2.7](#327-marking-a-hash-avalanching-from-outside)):

```cpp
template <>
struct std::hash<id> {
    using is_avalanching = void;

    auto operator()(id const& x) const noexcept -> size_t {
        return ankerl::unordered_dense::detail::wyhash::hash(x.value);
    }
};
```


#### 3.2.6. Hash the Whole Memory

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

#### 3.2.7. Marking a Hash Avalanching From Outside

`using is_avalanching = void;` is a member of the hash, which is no help when the hash comes from a library you cannot edit. `hash_is_avalanching` is what the map and set actually ask, and it can be answered from outside:

```cpp
template <>
struct ankerl::unordered_dense::hash_is_avalanching<their::good_hash> : std::true_type {};
```

The extra mixing is now skipped for `their::good_hash` everywhere, without touching it. The specialization also works the other way — `std::false_type` makes the map mix a hash's output whatever the hash claims about itself, which is the escape hatch for one that promises more than it delivers.

This is deliberately the same name, the same two ways of answering, and the same meaning as [`boost::hash_is_avalanching`](https://www.boost.org/doc/libs/latest/libs/unordered/doc/html/unordered/reference/hash_traits.html), so a hash annotated for Boost.Unordered is read correctly here and the other way around. The member may therefore also be written as a compile time bool, which is the spelling Boost's documentation asks for:

```cpp
using is_avalanching = std::true_type;   // same as `= void`
using is_avalanching = std::false_type;  // says the opposite
```

Boost calls `= void` deprecated; here it stays the ordinary spelling, since it is what this library has always documented and what every hash in the header uses. Writing anything else there — a stray `int`, say — is a compile error rather than a silent yes or no.

#### 3.2.8. Requiring an Avalanching Hash

In a codebase where every hash is meant to be a high quality one, forgetting to say so is the easy mistake, and nothing complains — the map just quietly mixes. Wrap the hash to make it a build error instead:

```cpp
template <class Key, class T>
using my_map = ankerl::unordered_dense::map<Key, T, ankerl::unordered_dense::require_avalanching<my_hash<Key>>>;
```

The requirement is written into the alias rather than next to the hash, so it is part of what `my_map` *is*, and survives `my_hash` being reimplemented without its marker — which a `static_assert` next to the hash does not. (It does not follow a map that is given a different hash outright: `map<K, V, other_hash>` names no requirement, so there is none.) It accepts a hash marked either way, by its own member typedef or by a `hash_is_avalanching` specialization.

The hash must not be `final`, since the wrapper derives from it — for one that is, specialize `hash_is_avalanching` instead. A stateful hash goes in either braced or by value: `require_avalanching<my_hash>{my_hash{seed}}`.

### 3.3. Container API

In addition to the standard `std::unordered_map` API (see https://en.cppreference.com/w/cpp/container/unordered_map), we have additional API that is somewhat similar to the node API, but leverages the fact that we're using a random access container internally:

#### 3.3.1. `auto replace_key(iterator it, K&& new_key) -> std::pair<iterator, bool>`

Updates the key of an element in-place without changing its position in the underlying container. This operation maintains iterator and reference stability - all existing iterators and references remain valid after the update.

Note that this can also be used as an optimization for `unordered_dense::set` when you want to `erase` one element and then `insert` a new element, this should be quite a bit faster.

#### 3.3.2. `auto extract() && -> value_container_type`

Extracts the internally used container. `*this` is emptied.

#### 3.3.3. `extract()` Single Elements

Similar to `erase()`, there is an API call `extract()`. It behaves exactly the same as `erase`, except that the return value is the moved element that is removed from the container:

* `auto extract(const_iterator it) -> value_type`
* `auto extract(Key const& key) -> std::optional<value_type>`
* `template <class K> auto extract(K&& key) -> std::optional<value_type>`

Note that the `extract(key)` API returns an `std::optional<value_type>` that is empty when the key is not found.

#### 3.3.4. `[[nodiscard]] auto values() const noexcept -> value_container_type const&`

Exposes the underlying values container.

#### 3.3.5. `auto replace(value_container_type&& container)`

Discards the internally held container and replaces it with the one passed. Non-unique elements are
removed, and the container will be partly reordered when non-unique elements are found.

#### 3.3.6. `auto hash_for(K const& key) const -> precomputed_hash`

Hashing a key is usually the largest part of a lookup, and looking up the same key over and over hashes it every time. `hash_for()` does it once, and `find`, `contains`, `count`, `equal_range` and `at` each take what it returns as a second argument:

```cpp
auto map = ankerl::unordered_dense::map<std::string, int>();
// ...

// hash it once, e.g. at startup
auto const status_hash = map.hash_for("status");

// as often as you like
auto it = map.find("status", status_hash);
```

The key is still needed — a lookup that lands on a bucket still has to compare keys to know it found the right one. What is skipped is the hashing, so the longer the key the more there is to gain (clang 18, x86-64, half hits and half misses):

| key length | `find(key)` | `find(key, hash)` | |
| ---------: | ----------: | ----------------: | ---: |
| 8 bytes | 5.6 ns | 4.0 ns | 1.4x |
| 32 bytes | 7.1 ns | 4.3 ns | 1.7x |
| 200 bytes | 22.5 ns | 7.4 ns | 3.0x |

`precomputed_hash` is a distinct type rather than a plain integer, because the number a lookup wants is *not* what `hash_function()` returns — the table finalizes that further — and an integer parameter would happily accept the wrong one. An integer does not convert to it; the value inside stays reachable, so a hash can be stored or moved around freely.

A hash belongs to the hasher, not to the table it came from. It stays valid across insertions, erasures, `rehash()` and moves, and every table using the same hasher takes it — so one hash can serve a map and a set together:

```cpp
auto set = ankerl::unordered_dense::set<std::string>();
auto found = set.find("status", status_hash); // the hash from the map above
```

What it does not survive is the key changing. Looking up a key with the hash of a different key does not throw or crash — it just quietly reports the key as not present.

Heterogeneous lookup works as usual when the hash and equality are transparent, and the hash may be taken from one key type and used with another:

```cpp
auto const h = map.hash_for(std::string_view("status"));
auto it = map.find("status"s, h);
```

Only lookups take a precomputed hash, and insertion never will: a lookup given the wrong hash merely misses, while an insertion given one files the element under a probe chain it is not on, losing it for good and letting a second copy of the same key in beside it. Erase is left out for a duller reason — it hashes the moved element as well as the key, so precomputing the key's hash would save it only half its hashing.

#### 3.3.7. `auto visit(FwdIt first, FwdIt last, F f) -> size_t`

Looks up a whole range of keys, calls `f` on each one that is there, and returns how many that was.

```cpp
auto const keys = std::vector<std::string>{"alpha", "beta", "gamma"};
auto total = 0;
auto found = map.visit(keys.begin(), keys.end(), [&](auto const& kv) { total += kv.second; });
```

`f` receives `value_type&`, or `value_type const&` on a `const` map, so a visit can modify what it finds. Keys that are absent are not reported; the count says how many were there.

**Why it is faster than the same loop of `find()`.** A lookup on a table past the cache is two dependent memory accesses — the group's block, and then the value the slot points at — and a loop doing one lookup at a time can only overlap them as far as the processor's own reordering reaches past a whole loop body. `visit` works a chunk at a time in three passes: every key's block is asked for, then the fingerprints are matched once the blocks have arrived, then the keys are compared. Every block in the chunk is in flight at once.

`map<uint64_t, size_t>`, clang 22 on a 7950X, ns per lookup, against the same batch looked up one key at a time:

| entries | | one at a time | `visit` | |
| ------: | :--- | ----: | ----: | ---: |
| 4 000 000 | all hits | 34.0 | 26.3 | 1.29x |
| 4 000 000 | half hits | 34.4 | 28.4 | 1.21x |
| 16 000 000 | all hits | 36.5 | 30.4 | 1.20x |
| 16 000 000 | half hits | 37.3 | 31.5 | 1.18x |

**It needs a table past the cache to be worth anything**, like every other memory-level trick here: on a map that fits in L2 there is nothing to overlap and the extra passes are a small loss.

**And the batching itself matters more than `visit` does.** If the keys are being fetched from somewhere in the same loop that looks them up — a random index into another array, say — then the key's own cache miss sits in front of the map's and neither overlaps with anything. Collecting the keys first and looking them up afterwards is worth **1.5x** at four million entries before `visit` is involved at all:

```cpp
for (size_t i = 0; i < n; ++i) {              // 51 ns per lookup
    auto it = map.find(keys[indices[i]]);
}

std::vector<key_type> batch;                  // 33 ns per lookup
for (size_t i = 0; i < n; ++i) { batch.push_back(keys[indices[i]]); }
for (auto const& k : batch) { auto it = map.find(k); }
```

That is a property of loops and memory parallelism rather than of this map, and it is the larger of the two effects. `visit` is what is left on top once the loop is already shaped that way.

#### 3.3.8. `void merge(map& source)`

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

### 3.4. Custom Container Types

`unordered_dense` accepts a custom allocator, but you can also specify a custom container for that template argument. That way it is possible to replace the internally used `std::vector` with e.g. `std::deque` or any other container like `boost::interprocess::vector`. This supports fancy pointers (e.g. [offset_ptr](https://www.boost.org/doc/libs/1_80_0/doc/html/interprocess/offset_ptr.html)), so the container can be used with e.g. shared memory provided by `boost::interprocess`.

### 3.5. Custom Bucket Types

The index is groups of sixteen slots; the bucket type chooses how wide a value index is. The
default should be good for pretty much everyone. See [5. Design](#5-design) for how the index
works.

#### 3.5.1. `ankerl::unordered_dense::bucket_type::group`

* Up to 2^32 = 4.29 billion elements.
* 5.5 bytes overhead per slot: one 88 byte block per group of sixteen slots, holding the sixteen fingerprints, the group's eight overflow counters and sixteen 4 byte value indices.

#### 3.5.2. `ankerl::unordered_dense::bucket_type::group_big`

* Up to 2^63 = 9,223,372,036,854,775,808 elements.
* 9.5 bytes overhead per slot: the same block with 8 byte value indices instead of 4 byte ones, so 152 bytes per group.

### 3.6. Disabling the Vector Probe

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

### 3.7. LLDB Data Formatters

The repository ships a formatter script for LLDB in [`lldb/unordered_dense.py`](lldb/unordered_dense.py). It makes
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
iteration order — insertion order until something is erased — and children are named after their key when the key
renders as a short scalar or string (`v map[2]` works by index regardless). The script only reads memory, so it is
safe on core dumps, and `frame variable -R <var>` still shows the raw members whenever they are wanted. Naming
children by key can be turned off with

```
script unordered_dense.NAME_CHILDREN_BY_KEY = False
```

Custom value containers (see [3.4](#34-custom-container-types)) fall back to whatever LLDB itself can display for
them.

### 3.8. Huge Pages

A lookup touches two or three random addresses -- the group's block, the value it points at, and for a string key its body -- and on 4 KB pages each of them is an address translation. A first-level data TLB holds on the order of 64-96 entries, a few hundred KB, so any table larger than that pays an L2 TLB lookup per access, and on a dependent chain that lookup is latency. Measured on this library's own scored benchmark by running the same binary with its heap on 2 MB pages: **2.6% (gcc) to 3.6% (clang) over the whole score, 5-8% on churn and on random finds at 50000 entries, and 22% of a lookup past the last-level cache**. Nothing asks for huge pages by default, and on the common Linux setting (`/sys/kernel/mm/transparent_hugepage/enabled` = `madvise`) nothing gets them without asking. There are two ways to ask.

**The environment, for the whole process.** glibc 2.35 and later can `madvise` its heap for you:

```sh
GLIBC_TUNABLES=glibc.malloc.hugetlb=1 ./your_program
```

This is what the numbers above were measured with, and it is the route that helps *small* tables too, because neighbouring blocks share a 2 MB extent on the heap. Setting the THP mode to `always` does the same for every process on the machine.

**The allocator, for one map.** `include/ankerl/huge_page_allocator.h` is a separate header (it needs `<sys/mman.h>`) with an allocator that puts every block of 2 MB and up on its own 2 MB-aligned, `MADV_HUGEPAGE`d mapping and leaves smaller ones to `std::allocator`:

```cpp
#include <ankerl/huge_page_allocator.h>

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
* **Every block is rounded up to 2 MB, and the rounding is resident memory**, because touching one byte of an `MADV_HUGEPAGE`d extent populates all of it. The map doubles both regions, so the loss is at most half a doubling step per region, while that region sits between doublings. The threshold is the second template parameter (`huge_page_allocator<T, 4 << 20>`), and it cannot go below 2 MB.

**With `segmented_vector`, size the segment for the page.** A segmented map never reallocates its values, so a segment on a huge page has no rounding loss to amortize and no copy to pay. The segment size is chosen through the container slot rather than the `segmented_map` alias:

```cpp
ankerl::unordered_dense::map<K, V, ankerl::unordered_dense::hash<K>, std::equal_to<K>,
    ankerl::unordered_dense::segmented_vector<std::pair<K, V>,
        ankerl::unordered_dense::huge_page_allocator<std::pair<K, V>>, 16 << 20>>
```

A segment holds a power of two of elements, rounded *down* to fit the byte size, so a 2 MB segment is exactly one huge page for a 16 byte pair and 1.28 MB -- below the threshold, no huge page -- for a 40 byte one. 16 MB segments bound that rounding at 2 MB each for any element size, and are the setting to use unless `sizeof(value_type)` is a power of two. Measured at 800000 entries, ns per operation, `std::vector` on `std::allocator` / segmented on this allocator with 16 MB segments: build `uint64_t` 22.0 / 14.7, `std::string` 85.6 / **63.3**, 64 byte values 77.1 / **20.5**; churn `uint64_t` 16.1 / 14.5, `std::string` 101.0 / 95.6. The segmented container costs 10-20% on lookups and churn for its extra indirection, and huge pages do not take that back; on builds it is the fastest thing here, because it neither copies nor faults.

On Windows and macOS the class exists with the same interface and forwards everything to `std::allocator`; `huge_page_allocator<T>::uses_huge_pages` says which you got. `scripts/ab/huge_pages.sh` measures it across sizes and workloads, and the measurements are in `notes/index-design.md`.

## 4. `segmented_map` and `segmented_set`

`ankerl::unordered_dense` provides a custom container implementation that has lower memory requirements than the default `std::vector`. Memory is not contiguous, but it can allocate segments without having to reallocate and move all the elements. In summary, this leads to

* Much smoother memory usage of the values, which increases continuously.
* No high peak memory usage from the values.
* Faster insertion because elements never need to be moved to newly allocated blocks
* Slightly slower indexing compared to `std::vector` because an additional indirection is needed.

Here is what each of four maps holds while 10 million `uint64_t -> uint64_t` pairs are inserted into it:
![allocated memory](doc/allocated_memory.png)

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

## 5. Design

The map/set has two data structures:
* `std::vector<value_type>` which holds all data. map/set iterators are just `std::vector<value_type>::iterator`!
* An indexing structure, which is a flat array of blocks. Each block is one group of sixteen slots: their fingerprints, the group's overflow counters, and the sixteen value indices, all in the same 88 bytes.

### 5.1. Inserts

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

### 5.2. Lookups

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
arithmetic, see [3.6. Disabling the Vector Probe](#36-disabling-the-vector-probe).

### 5.3. Removals

Since all data is stored in a vector, removals are a bit more complicated:

1. First, look up the element to delete in the index.
2. Clear its fingerprint, and decrement the overflow counter in every group between its home group
   and the one it landed in. An erase undoes exactly what the insert did, so there are no
   tombstones and no rehash is ever needed to repair the index. Nothing else moves.
3. Replace that element in the vector with the last element in the vector.
4. Update the slot of the moved element, which requires another lookup.

## 6. Real World Usage

Open source projects that use this map, grouped by what they do. The list was first put together on 2023-09-10 and last refreshed on 2026-08-06; every entry was confirmed by finding the include or the namespace in the project's own source on its default branch. Some authors have written in, the rest come from searching GitHub. Please send me a note if you want to be on that list!

### 6.1. Databases and data engines

* [AliSQL](https://github.com/alibaba/AliSQL) - A MySQL branch originated from Alibaba Group.
* [ArcticDB](https://github.com/man-group/ArcticDB) - A high performance, serverless DataFrame database built for the Python Data Science ecosystem.
* [Bodo](https://github.com/bodo-ai/Bodo) - A high performance compute engine for Python data processing.
* [Milvus](https://github.com/milvus-io/milvus) - A high-performance, cloud-native vector database built for scalable vector search.
* [MySQL](https://github.com/mysql/mysql-server) - Binary log transaction dependency tracking has used this map since 8.4.3 and 9.1.0, replacing a tree for the writeset history and taking about 60% less space for it.
* [Percona Server](https://github.com/percona/percona-server) - A free, fully compatible, enhanced and open source drop-in replacement for MySQL.
* [Percona XtraBackup](https://github.com/percona/percona-xtrabackup) - Open source hot backup tool for InnoDB and XtraDB databases.
* [RonDB](https://github.com/logicalclocks/rondb) - A distribution of NDB Cluster for real-time applications with high availability.

### 6.2. Games, emulators and game engines

* [Citron](https://github.com/citron-neo/emulator) - A Nintendo Switch emulator.
* [CrystalEngine](https://github.com/neilmewada/CrystalEngine) - A Vulkan game engine with FrameGraph, PBR rendering and a declarative UI framework.
* [DevilutionX](https://github.com/diasurgical/DevilutionX) - Diablo build for modern operating systems.
* [FEX](https://github.com/FEX-Emu/FEX) - A fast usermode x86 and x86-64 emulator for Arm64 Linux.
* [FOnline Engine](https://github.com/cvet/fonline) - A flexible cross-platform isometric game engine for multiplayer games.
* [HiveWE](https://github.com/stijnherfst/HiveWE) - A Warcraft III World Editor (WE) that focusses on speed and ease of use.
* [impacto](https://github.com/CommitteeOfZero/impacto) - A reimplementation of the "MAGES." visual novel engine.
* [LandSandBoat](https://github.com/LandSandBoat/server) - A server emulator for Final Fantasy XI.
* [Marathon Recompiled](https://github.com/sonicnext-dev/MarathonRecomp) - An unofficial PC port of the Xbox 360 version of Sonic the Hedgehog (2006), created via static recompilation.
* [Nazara Engine](https://github.com/NazaraEngine/NazaraEngine) - A cross-platform framework aimed at (but not limited to) real-time applications and games.
* [NVGT](https://github.com/samtupy/nvgt) - The Nonvisual Gaming Toolkit, a cross-platform audio game engine.
* [Oxylus Engine](https://github.com/oxylusengine/Oxylus) - A data-driven Vulkan game engine built in C++.
* [Project Alice](https://github.com/schombert/Project-Alice) - An open source recreation of the grand strategy game Victoria II.
* [Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp) - An unofficial PC port of the Xbox 360 version of Sonic Unleashed, created via static recompilation.
* [Visual Pinball](https://github.com/vpinball/vpinball) - An open source pinball table editor and simulator.

### 6.3. Graphics, rendering and GPU compute

* [AdaptiveCpp](https://github.com/AdaptiveCpp/AdaptiveCpp) - Compiler for multiple programming models (SYCL, C++ standard parallelism) for CPUs and GPUs from all vendors.
* [CyberFSR2](https://github.com/PotatoOfDoom/CyberFSR2) - Drop-in DLSS replacement with FSR 2.0 for various games such as Cyberpunk 2077.
* [D3D12_Research](https://github.com/simco50/D3D12_Research) - A hobby project to experiment with various modern rendering techniques in DirectX 12.
* [LuisaCompute](https://github.com/LuisaGroup/LuisaCompute) - High-performance rendering framework on stream architectures.
* [NVIDIA MDL SDK](https://github.com/NVIDIA/MDL-SDK) - The NVIDIA Material Definition Language SDK, for physically based material definitions in rendering applications.
* [OptiScaler](https://github.com/optiscaler/OptiScaler) - Bridges upscaling and frame generation across GPUs, supporting DLSS2+, XeSS and FSR2+ inputs.
* [Skyrim Community Shaders](https://github.com/community-shaders/skyrim-community-shaders) - Community-driven advanced graphics modifications for Skyrim AE, SE and VR.
* [Slang](https://github.com/shader-slang/slang) - A shading language that makes it easier to build and maintain large shader codebases in a modular and extensible fashion.
* [WinUI](https://github.com/microsoft/microsoft-ui-xaml) - A modern UI framework with a rich set of controls and styles, the native UI layer of the Windows App SDK.

### 6.4. Maps and geospatial

* [Cloudini](https://github.com/facontidavide/cloudini) - A point cloud compression library, with ROS/PCL integration.
* [CoMaps](https://codeberg.org/comaps/comaps) - Privacy-focused offline maps and navigation for Android and iOS, based on OpenStreetMap data.
* [HDMapping](https://github.com/MapsHD/HDMapping) - Open source software for mobile mapping, LiDAR odometry and point cloud registration.
* [MapLibre Native](https://github.com/maplibre/maplibre-native) - Interactive vector tile maps for iOS, Android and other platforms.
* [Valhalla](https://github.com/valhalla/valhalla) - Open source routing engine for OpenStreetMap data. Replaced robin-hood-hashing with this map and set in 3.6.0.

### 6.5. CAD, 3D printing and simulation

* [Bambu Studio](https://github.com/bambulab/BambuStudio) - PC software for BambuLab and other 3D printers.
* [Lethe](https://github.com/chaos-polymtl/lethe) - Open-source computational fluid dynamics (CFD) software which uses high-order continuous Galerkin formulations to solve the incompressible Navier–Stokes equations (among others).
* [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) - G-code generator for 3D printers (RepRap, Makerbot, Ultimaker etc.).
* [web-ifc](https://github.com/ThatOpen/engine_web-ifc) - Reading and writing IFC files with Javascript, at native speeds.

### 6.6. Bioinformatics

* [GW](https://github.com/kcleal/gw) - Genome browser and variant annotation tool for interactive visualisation of sequencing data.
* [kallisto](https://github.com/pachterlab/kallisto) - Near-optimal RNA-Seq quantification.
* [MashMap](https://github.com/marbl/MashMap) - A fast approximate aligner for long DNA sequences.
* [metaMDBG](https://github.com/GaetanBenoitDev/metaMDBG) - A lightweight assembler for long and accurate metagenomics reads.
* [wfmash](https://github.com/waveygang/wfmash) - Base-accurate DNA sequence alignments using WFA and mashmap3.

### 6.7. Networking, media and security

* [Kismet](https://github.com/kismetwireless/kismet) - A sniffer, WIDS and wardriving tool for Wi-Fi, Bluetooth, Zigbee and RF, which runs on Linux and macOS.
* [libossia](https://github.com/ossia/libossia) - A modern C++, cross-environment distributed object model for creative coding and interaction scoring.
* [mediasoup](https://github.com/versatica/mediasoup) - Cutting edge WebRTC video conferencing SFU.
* [ossia score](https://github.com/ossia/score) - A free, open-source, cross-platform intermedia sequencer for precise and flexible scripting of interactive scenarios.
* [Rspamd](https://github.com/rspamd/rspamd) - Fast, free and open-source spam filtering system.
* [YANET](https://github.com/yanet-platform/yanet) - A high performance framework for forwarding traffic based on DPDK.

### 6.8. Finance and blockchain

* [Cartesi Machine Emulator](https://github.com/cartesi/machine-emulator) - The off-chain RISC-V emulator implementation of the Cartesi Machine.
* [Monad](https://github.com/category-labs/monad) - A high-performance EVM-compatible layer-1 blockchain client.
* [opentxs](https://github.com/Open-Transactions/opentxs) - A free-software toolkit implementing the OTX protocol, together with a financial cryptography library, API, GUI, command-line interface and prototype notary server.
* [RISC Zero](https://github.com/risc0/risc0) - A zero-knowledge verifiable general computing platform based on RISC-V.
* [WonderTrader](https://github.com/wondertrader/wondertrader) - A one-stop quantitative research and trading framework.

### 6.9. Tools, libraries and machine learning

* [ArkScript](https://github.com/ArkScript-lang/Ark) - A small, fast, functional and scripting language for C++ projects.
* [File Commander](https://github.com/VioletGiraffe/file-commander) - A cross-platform Total Commander-like orthodox file manager for Windows, Mac and Linux.
* [FlashTokenizer](https://github.com/NLPOptimize/flash-tokenizer) - An efficient and optimized BERT tokenizer engine for LLM inference serving.
* [Ichor](https://github.com/volt-software/Ichor) - A C++20 microservice bootstrapping framework focused on thread safety and dependency injection.
* [minigpt4.cpp](https://github.com/Maknee/minigpt4.cpp) - Port of MiniGPT4 in C++ (4bit, 5bit, 6bit, 8bit, 16bit CPU inference with GGML).
* [Nimble Commander](https://github.com/mikekazakov/nimble-commander) - A dual-pane file manager for macOS.
* [Operon](https://github.com/heal-research/operon) - A modern C++ framework for symbolic regression that uses genetic programming to find the best-fitting model for a given regression target.
* [PECOS](https://github.com/amzn/pecos) - A versatile and modular machine learning framework for fast learning and inference on problems with large output spaces, such as extreme multi-label ranking and large-scale retrieval.
* [PlotJuggler](https://github.com/PlotJuggler/PlotJuggler) - The time series visualization tool that you deserve.
* [PyOptInterface](https://github.com/metab0t/PyOptInterface) - Efficient modeling interface for mathematical optimization in Python.
* [STP](https://github.com/stp/stp) - Simple Theorem Prover, an efficient SMT solver for bitvectors.
* [Tulip](https://github.com/Tulip-Dev/tulip) - Large graphs analysis, drawing and visualization framework.

### 6.10. Ports

Reimplementations of this design in other languages. They are not maintained here, and are listed because people have found them useful.

* [HashMapC99](https://github.com/benanil/HashMapC99) - A cache-efficient, densely stored hash map in C99, by Anılcan Gülkaya. Useful where a C++17 header is not an option, such as embedded targets, and for shorter compile times and smaller binaries.
