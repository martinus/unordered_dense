<a id="top"></a>

[![meson_build_test](https://github.com/martinus/unordered_dense/actions/workflows/main.yml/badge.svg)](https://github.com/martinus/unordered_dense/actions)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/martinus/unordered_dense/main/LICENSE)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/6220/badge)](https://bestpractices.coreinfrastructure.org/projects/6220)

# 🚀 ankerl::unordered_dense::{map, set}

A fast & densely stored hashmap and hashset based on robin-hood backward shift deletion.

The classes `ankerl::unordered_dense::map` and `ankerl::unordered_dense::set` are (almost) drop-in replacements of `std::unordered_map` and `std::unordered_set`.
While they don't have as strong iterator / reference stability guaranties, they are typically *much* faster. Here is a short summary of the properties:


## Advantages
* Perfect iteration speed - Data is stored in a `std::vector`, all data is contiguous!
* Very fast insertion & lookup speed, in the same ballpark as [`absl::flat_hash_map`](https://abseil.io/docs/cpp/guides/container`)
* Low memory usage
* Full support for `std::allocators`, and [polymmorphic allocators](https://en.cppreference.com/w/cpp/memory/polymorphic_allocator). There are `ankerl::unordered_dense::pmr` typedefs available
* Simple: single header with just a bit over 1000 lines of code, this is less than half of [robin-hood-hashing](https://github.com/martinus/robin-hood-hashing)

## Disadvantages
* Deletion speed is relatively slow. This needs two lookups: one for the element to delete, and one for the element that is moved onto the newly empty spot.
* no `const Key` in `std::pair<Key, Value>`
* Iterators are not stable on insert/erase

## Design

This is a newly implemented hashmap based on lessons learned from [robin-hood-hashing](https://github.com/martinus/robin-hood-hashing)

* Perfect iteration speed. Use an `std::vector<std::pair<Key, Value>>` (maybe customizable container) for the content that is always 100% compact.
* Use an indexing data of 8 byte where overflow is no issue:
   ```
    0 1 2 3 4 5 6 7 8
   +-+-+-+-+-+-+-+-+-+
   |dist |h|   idx   |
   +-+-+-+-+-+-+-+-+-+
   ```
    * `dist`: 3 byte offset from original bucket. 0 means empty, 1 means here, ... => 2^24-2 = 16777214 collisions possible, should be plenty
    * `h`: 1 byte hash. That's enough with high probability find the correct element without having to check the key. Very useful for e.g. large strings.
    * `dist` and `h` are treated together as a single 32bit `uint32_t`.
    * `4` byte index into the `std::vector<std::pair<Key, Value>>` dense storage. That's enough for 4294 million entries.
* with a reasonable mixer of the hash. E.g. [mumx](https://github.com/martinus/map_benchmark/blob/master/src/app/mixer.h#L43-L47)? Or just multiply with a random odd 64bit number and take the upper bits? Or both.
* C++17

# Expected Advantages / Disadvantages
