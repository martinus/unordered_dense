<a id="top"></a>

[![Release](https://img.shields.io/github/release/martinus/unordered_dense.svg)](https://github.com/martinus/unordered_dense/releases)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/martinus/unordered_dense/main/LICENSE)
[![meson_build_test](https://github.com/martinus/unordered_dense/actions/workflows/main.yml/badge.svg)](https://github.com/martinus/unordered_dense/actions)
[![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/6220/badge)](https://bestpractices.coreinfrastructure.org/projects/6220)
[![Sponsors](https://img.shields.io/github/sponsors/martinus?style=social)](https://github.com/sponsors/martinus)

# 🚀 ankerl::unordered_dense::{map, set} <!-- omit in toc -->

A fast & densely stored hashmap and hashset based on robin-hood backward shift deletion.

The classes `ankerl::unordered_dense::map` and `ankerl::unordered_dense::set` are (almost) drop-in replacements of `std::unordered_map` and `std::unordered_set`. While they don't have as strong iterator / reference stability guaranties, they are typically *much* faster. 

- [1. Overview](#1-overview)
- [2. Installation](#2-installation)
  - [2.1. Installing using cmake](#21-installing-using-cmake)
- [3. Extensions](#3-extensions)
  - [3.1. Container API](#31-container-api)
    - [3.1.1. `auto extract() && -> value_container_type`](#311-auto-extract----value_container_type)
    - [3.1.2. `[[nodiscard]] auto values() const noexcept -> value_container_type const&`](#312-nodiscard-auto-values-const-noexcept---value_container_type-const)
    - [3.1.3. `auto replace(value_container_type&& container)`](#313-auto-replacevalue_container_type-container)
  - [3.2. Custom Container Types](#32-custom-container-types)
  - [3.3. Custom Bucket Tyeps](#33-custom-bucket-tyeps)
    - [3.3.1. `ankerl::unordered_dense::bucket_type::standard`](#331-ankerlunordered_densebucket_typestandard)
    - [3.3.2. `ankerl::unordered_dense::bucket_type::big`](#332-ankerlunordered_densebucket_typebig)
- [4. Design](#4-design)
  - [4.1. Inserts](#41-inserts)
  - [4.2. Lookups](#42-lookups)
  - [4.3. Removals](#43-removals)

## 1. Overview

The chosen design has a few advantages over `std::unordered_map`: 

* Perfect iteration speed - Data is stored in a `std::vector`, all data is contiguous!
* Very fast insertion & lookup speed, in the same ballpark as [`absl::flat_hash_map`](https://abseil.io/docs/cpp/guides/container`)
* Low memory usage
* Full support for `std::allocators`, and [polymorphic allocators](https://en.cppreference.com/w/cpp/memory/polymorphic_allocator). There are `ankerl::unordered_dense::pmr` typedefs available
* Customizeable storage type: with a template parameter you can e.g. switch from `std::vector` to `boost::interprocess::vector` or any other compatible random-access container.

There's no free lunch, so there are a few disadvantages:

* Deletion speed is relatively slow. This needs two lookups: one for the element to delete, and one for the element that is moved onto the newly empty spot.
* no `const Key` in `std::pair<Key, Value>`
* Iterators are not stable on insert/erase


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

## 3. Extensions

### 3.1. Container API

In addition to the standard `std::unordered_map` API (see https://en.cppreference.com/w/cpp/container/unordered_map) we have additional API leveraging the fact that we're using a random access container internally:

#### 3.1.1. `auto extract() && -> value_container_type`

Extracts the internally used container. `*this` is emptied.

#### 3.1.2. `[[nodiscard]] auto values() const noexcept -> value_container_type const&`

Exposes the underlying values container.

#### 3.1.3. `auto replace(value_container_type&& container)`

Discards the internally held container and replaces it with the one passed. Non-unique elements are
removed, and the container will be partly reordered when non-unique elements are found.

### 3.2. Custom Container Types

`unordered_dense` accepts a custom allocator, but you can also specify a custom container for that template argument. That way it is possible to replace the internally used `std::vector` with e.g. `std::deque` or any other container like `boost::interprocess::vector`. This supports fancy pointers (e.g. [offset_ptr](https://www.boost.org/doc/libs/1_80_0/doc/html/interprocess/offset_ptr.html)), so the container can be used with e.g. shared memory provided by `boost::interprocess`.

### 3.3. Custom Bucket Tyeps

The map/set supports two different bucket types. The default should be good for pretty much everyone.

#### 3.3.1. `ankerl::unordered_dense::bucket_type::standard`

* Up to 2^32 = 4.29 billion elements.
* 8 bytes overhead per bucket.

#### 3.3.2. `ankerl::unordered_dense::bucket_type::big`

* up to 2^63 = 9223372036854775808 elements.
* 12 bytes overhead per bucket.

## 4. Design

The map/set has two data structures:
* `std::vector<value_type>` which holds all data. map/set iterators are just `std::vector<value_type>::iterator`!
* An indexing structure (bucket array), which is a flat array with 8-byte buckets.

### 4.1. Inserts

Whenever an element is added it is `emplace_back` to the vector. The key is hashed, and an entry (bucket) is added at the
corresponding location in the bucket array. The bucket has this structure:

```cpp
struct Bucket {
    uint32_t dist_and_fingerprint;
    uint32_t value_idx;
};
```

Each bucket stores 3 things:
* The distance of that value from the original hashed location (3 most significant bytes in `dist_and_fingerprint`)
* A fingerprint; 1 byte of the hash (lowest significant byte in `dist_and_fingerprint`)
* An index where in the vector the actual data is stored.

This structure is especially designed for the collision resolution strategy robin-hood hashing with backward shift
deletion.

### 4.2. Lookups

The key is hashed and the bucket array is searched if it has an entry at that location with that fingerprint. When found,
the key in the data vector is compared, and when equal the value is returned.

### 4.3. Removals

Since all data is stored in a vector, removals are a bit more complicated:

1. First, lookup the element to delete in the index array.
2. When found, replace that element in the vector with the last element in the vector. 
3. Update *two* locations in the bucket array: First remove the bucket for the removed element
4. Then, update the `value_idx` of the moved element. This requires another lookup.

