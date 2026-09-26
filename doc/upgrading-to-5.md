# Upgrading from 4.x to 5.0

## Most projects do nothing

If you use `map` or `set` with the default template arguments, and you do not store hash values
anywhere outside the process, then 5.0 is a recompile. Everything in the container API is
unchanged: `try_emplace`, `insert_or_assign`, `erase`, `extract`, `replace`, `values`,
`hash_for`, `replace_key`, the `hash<T>` specialization protocol, `is_avalanching`,
`max_load_factor`, the allocator and container template arguments, and the `pmr` aliases all mean
what they meant in 4.11.0.

The rest of this page is for the projects that do one of six things.

## 1. You named a bucket type

The index is no longer robin hood, so the bucket types are named after what they now are.

```cpp
// 4.x
ankerl::unordered_dense::map<K, V, Hash, Eq, Alloc, ankerl::unordered_dense::bucket_type::standard>;
ankerl::unordered_dense::map<K, V, Hash, Eq, Alloc, ankerl::unordered_dense::bucket_type::big>;

// 5.0
ankerl::unordered_dense::map<K, V, Hash, Eq, Alloc, ankerl::unordered_dense::bucket_type::group>;
ankerl::unordered_dense::map<K, V, Hash, Eq, Alloc, ankerl::unordered_dense::bucket_type::group_big>;
```

`group` holds up to 2^32 values and `group_big` up to 2^63, which is the same split `standard` and
`big` made. Both are mechanical renames:

```sh
git ls-files -z '*.h' '*.hpp' '*.cpp' '*.cc' '*.cxx' | xargs -0 sed -i \
    -e 's/bucket_type::standard/bucket_type::group/g' \
    -e 's/bucket_type::big/bucket_type::group_big/g'
```

## 2. You passed a seventh template argument to `map` or `set`

The `BucketContainer` parameter is gone. The index is one contiguous array in every configuration
now, so there was nothing left for it to choose.

```cpp
// 4.x - seven parameters
template <class Key, class T, class Hash, class KeyEqual, class AllocatorOrContainer,
          class Bucket, class BucketContainer>

// 5.0 - six
template <class Key, class T, class Hash, class KeyEqual, class AllocatorOrContainer,
          class Bucket>
```

If you passed it, delete the argument. `detail::default_container_t`,
`detail::default_bucket_container_type` and `detail::dist_and_fingerprint_type` went with it.

## 3. You passed a seventh template argument to `segmented_map` or `segmented_set`

This one is not a deletion, it is a different kind of parameter. The seventh slot used to be a type
and is now a `std::size_t`:

```cpp
// 4.x
template <..., class Bucket, class BucketContainer>

// 5.0
template <..., class Bucket, std::size_t MaxSegmentSizeBytes = 4096>
```

A segment size that is not the default is the only reason to name it. Passing a type there is a
compile error rather than a silent change in behaviour, so the compiler will find these for you.

## 4. You called the hash directly

The two overloads of `detail::wyhash::hash` became two differently named functions, because the
implementation is no longer wyhash and the two are no longer the same algorithm:

```cpp
// 4.x
ankerl::unordered_dense::detail::wyhash::hash(ptr, len);   // bytes
ankerl::unordered_dense::detail::wyhash::hash(x);          // a uint64_t

// 5.0
ankerl::unordered_dense::detail::hash_bytes(ptr, len);
ankerl::unordered_dense::detail::hash_int(x);
```

**Do not sed this one.** Both 4.x spellings are the same token, and which replacement is right
depends on the argument. Compile, and fix what the compiler points at.

## 5. You store hash values outside the process

This break and the next one are the two that are not compile errors. This is the one to take
seriously.

**The hash produces different values in 5.0.** Anything you persisted, sent over a wire, or used as
a cache key or shard index has to be recomputed. A file written by 4.x and read by 5.0 will not
report an error, it will simply not find things.

This cannot be detected for you. If you are not sure whether your project does it, grep for
anywhere a hash value leaves the process.

## 6. You sized the index with `bucket_count() * sizeof(bucket_type)`

This one still compiles, still runs, and is off by up to 4.36x.

In 4.x the index was one array with one bucket per slot, so that product was the index, exactly. In
5.0 neither half means that any more. `bucket_count()` still counts slots, but slots come in groups
of sixteen, and `bucket_type` is only the part of a group that gets compared: sixteen fingerprints
and eight overflow counters, 24 bytes. The sixteen value indices sit in the same block and are not
part of the type. The product is therefore 24 bytes per slot no matter what the index costs.

Measured with a counting allocator, `map<uint64_t, uint64_t>` filled with a million entries, which
is 2097152 slots:

| version, bucket type | `sizeof(bucket_type)` | the product | the index's live bytes | overstated by |
|---|---|---|---|---|
| 4.4.0, `standard` | 8 | 8.00 B/slot | 8.00 B/slot | 1.00x |
| 5.0.1, `group` | 24 | 24.00 B/slot | 5.50 B/slot | 4.36x |
| 5.0.1, `group_big` | 24 | 24.00 B/slot | 9.50 B/slot | 2.53x |

Notably, `sizeof(bucket_type)` is 24 for both bucket types in 5.0, so the product cannot tell them
apart although they differ by 4 bytes per slot.

Nothing reports this, which means that it matters most where the number is used rather than printed.
E.g. MySQL's
[hash join](https://github.com/mysql/mysql-server/blob/trunk/sql/iterators/hash_join_buffer.cc) adds
this product to the size of its row buffer and spills the join to disk once the total goes over
`join_buffer_size`. It is on 4.4.0, where the product is exact.

Ask the map instead. `index_bytes()` is the number, for either bucket type:

```cpp
auto index = map.index_bytes();
auto values = map.values().capacity() * sizeof(decltype(map)::value_type);
```

`values()` is unchanged from 4.x, so only the first line is new. Both numbers are bytes asked for,
not resident pages, and they do not count what the allocator rounds up or what a growth keeps around
while it copies. In 5.0.0 and 5.0.1, which do not have the accessor, `bucket_count() / 16 * 88` is
the same number for `bucket_type::group` and `/ 16 * 152` for `group_big`.

## Also worth knowing, though it is not an API change

`segmented_map` segments its values and no longer segments its index. References into the values
are still stable and the values still grow smoothly, which is what `segmented_map` is for. What
changed is that growing the index now allocates the new one beside the old, so a
`segmented_map<uint64_t, uint64_t>` reaching 100 million entries has a transient of about 1.1 GB
that 4.x did not have. `reserve()` up front removes it.

## Checking which version you compiled against

```cpp
#if ANKERL_UNORDERED_DENSE_VERSION_MAJOR >= 5
    // 5.0 or newer
#endif
```

The inline namespace follows the version, so `ankerl::unordered_dense::v5_1_0` is also the exact
version, and linking two translation units built against different versions is a link error rather
than a silent mismatch.

## What you get for the trouble

Against 4.11.0, measured on a Ryzen 9 7950X: `uint64_t` keys find about 1.5x faster and build up to
2.3x faster, `std::string` keys 1.0x to 1.3x, and memory overhead is 5.5 bytes per slot instead of
8. Iteration does not move, because 4.11.0 already stored its values in a `std::vector` and that is
where iteration speed came from. The [release notes](https://github.com/martinus/unordered_dense/releases/tag/v5.0.0)
have the full tables and the cases where it does not help.

There are also four things that did not exist in 4.x: `merge()`, a bulk `visit(first, last, f)`,
the opt-in [huge page allocator](usage.md#huge-pages), and a probe that is bounded for every hash,
including one an attacker chose. The last one is a correctness fix, not a feature: in 4.x, eight
chosen keys were enough to make `contains()` on an absent key loop forever.
