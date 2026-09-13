[![Release](https://img.shields.io/github/release/martinus/unordered_dense.svg)](https://github.com/martinus/unordered_dense/releases)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/martinus/unordered_dense/main/LICENSE)
[![meson_build_test](https://github.com/martinus/unordered_dense/actions/workflows/main.yml/badge.svg)](https://github.com/martinus/unordered_dense/actions)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/6220/badge)](https://bestpractices.coreinfrastructure.org/projects/6220)
[![Sponsors](https://img.shields.io/github/sponsors/martinus?style=social)](https://github.com/sponsors/martinus)

# 🚀 ankerl::unordered_dense::{map, set}

A fast & densely stored hashmap and hashset for C++17 and later.

The classes `ankerl::unordered_dense::map` and `ankerl::unordered_dense::set` are (almost) drop-in replacements of `std::unordered_map` and `std::unordered_set`. While they don't have as strong iterator / reference stability guarantees, they are typically *much* faster.

Additionally, there are `ankerl::unordered_dense::segmented_map` and `ankerl::unordered_dense::segmented_set` with lower peak memory usage, and stable references (iterators are NOT stable) on insert.

A word count, with `map` in its default configuration:

```cpp
#include <ankerl/unordered_dense.h>

#include <iostream>
#include <string>

auto main() -> int {
    auto counts = ankerl::unordered_dense::map<std::string, int>();
    for (auto const* word : {"the", "quick", "brown", "fox", "the"}) {
        ++counts[word];
    }

    // iterating walks a std::vector, in insertion order
    for (auto const& [word, count] : counts) {
        std::cout << count << ' ' << word << '\n';
    }
}
```

## Dense storage, and what it costs

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

## Benchmarks

Obviously this is my own map's README, so the bias is where you'd expect it. Rows are sorted by the geometric mean of all five panels, and one of the five is `iterate`, which a dense map wins by 5.5x to 13x. That column decides most of the order on its own. Sorted by `find` instead, this map is seventh of fourteen.

Every map runs in the configuration you get by typing its type name, own hash included. Everything is relative to `ankerl::unordered_dense::map`, so 1.00 is level with it and 2.00 is twice the cost. Ryzen 9 7950X, clang 22.1.8, one binary per map, one million to two million entries. Raw numbers are in [doc/bench_readme.csv](doc/bench_readme.csv).

![benchmark results, uint64_t keys](doc/bench-readme-u64.svg)

![benchmark results, std::string keys](doc/bench-readme-str.svg)

In short: iteration is what the dense layout buys, 0.19 ns per element against 5.5x to 13x for the flat maps and 110x for `std::unordered_map`. With `std::string` keys it builds and destroys 2.1x to 2.6x faster than any flat map. Integer `find` and `churn` are what that costs, 0.73 and 0.61 against `boost::unordered_flat_map`.

[doc/benchmarks.md](doc/benchmarks.md) has what each panel measures, what huge pages and `segmented_map` do to the same numbers, how the run was taken and what it does not say.

## Installation

The map is header-only. Copy `include/ankerl/unordered_dense.h` and `include/ankerl/stl.h` into your project, keeping them in the same directory, and include `unordered_dense.h`. `stl.h` holds nothing but the standard includes, split out so that a build using `import std` can skip it.

<!-- See https://github.com/bernedom/SI/blob/main/doc/installation-guide.md -->
Or install it. The default installation location is `/usr/local`. Clone the repository and run these commands in the cloned folder:

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

## Documentation

* [Usage](doc/usage.md) - the hash, the API a vector of values makes possible, and the shapes `map` and `set` can be asked to take.
* [Design](doc/design.md) - how the index works: one 88 byte block per group of sixteen slots, the overflow counters that make tombstones unnecessary, and what an insert, a lookup and an erase do.
* [Benchmarks](doc/benchmarks.md) - the long version of the two graphs above.
* [Real world usage](doc/users.md) - the open source projects that use this map, from MySQL to PrusaSlicer.
* [scripts/ab](scripts/ab/README.md) - the measurement harnesses. Every result they have produced is written down in [notes/index-design.md](notes/index-design.md), the negative ones included.
