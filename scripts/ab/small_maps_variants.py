#!/usr/bin/env python3
"""Writes the #304 prototype headers that small_maps.sh compares, each into OUT/<name>/ankerl.

    scripts/ab/small_maps_variants.py OUT
    AB_BUILD=OUT scripts/ab/small_maps.sh 5 main=OUT/main two=OUT/two A8=OUT/A8 A16=OUT/A16 B8=OUT/B8

main  the header as it is
two   the smallest index two groups instead of four (initial_shifts 63); one group would need
      hash >> 64, which is undefined, or a mask on every lookup
A<n>  no index for the first n values: an insert scans them and appends, value n+1 allocates the
      index and places them all; find tests for "no index" before hashing and scans
B8    the same insert; find hashes and probes the sentinel, and scans only on a miss

Prototypes for build, count and destroy only: erase, extract, replace, merge and the rest do not
know the no-index mode, so the unit tests do not apply to them. The switch to an index hashes every
value once to place it and the inserted key a second time.
"""
import os
import shutil
import sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
src = open(os.path.join(root, "include/ankerl/unordered_dense.h")).read()
stl = os.path.join(root, "include/ankerl/stl.h")


def write(out, name, text):
    d = os.path.join(out, name, "ankerl")
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "unordered_dense.h"), "w").write(text)
    shutil.copy(stl, d)


def replace(text, old, new):
    assert text.count(old) == 1, old[:70]
    return text.replace(old, new)


SCAN = """for (std::size_t i = 0; i < m_values.size(); ++i) {
                if (m_equal(key, get_key(m_values[i]))) {
                    return begin() + static_cast<difference_type>(i);
                }
            }
            return end();"""


def small(n, mode):
    s = replace(src, """        allocate_buckets_if_none();
        auto const counter = word & 7U;
        if (ANKERL_UNORDERED_DENSE_LIKELY(m_buckets.data()[home_idx].m_overflows[counter] == 0)) {""",
                """        if (m_buckets.empty()) {
            if (m_values.size() < %d) {
                for (std::size_t i = 0; i < m_values.size(); ++i) {
                    if (m_equal(key, get_key(m_values[i]))) {
                        return {begin() + static_cast<difference_type>(i), false};
                    }
                }
                if constexpr (Piecewise) {
                    append_value(std::piecewise_construct,
                                 std::forward_as_tuple(std::forward<K>(key)),
                                 std::forward_as_tuple(std::forward<Args>(args)...));
                } else {
                    append_value(std::forward<Args>(args)...);
                }
                return {begin() + static_cast<difference_type>(m_values.size() - 1), true};
            }
            allocate_buckets_if_none();
            fill_buckets_from_values();
            return do_find_or_place<Piecewise>(std::forward<K>(key), std::forward<Args>(args)...);
        }
        auto const counter = word & 7U;
        if (ANKERL_UNORDERED_DENSE_LIKELY(m_buckets.data()[home_idx].m_overflows[counter] == 0)) {""" % n)
    old = """    auto do_find(K const& key) -> iterator {
        return do_find_hashed(key, mixed_hash(key));"""
    if mode == "A":
        new = """    auto do_find(K const& key) -> iterator {
        if (ANKERL_UNORDERED_DENSE_UNLIKELY(m_buckets.empty())) {
            """ + SCAN + """
        }
        return do_find_hashed(key, mixed_hash(key));"""
    else:
        new = """    auto do_find(K const& key) -> iterator {
        auto r = probe(key, mixed_hash(key));
        if (r.found) {
            return begin() + static_cast<difference_type>(r.value_idx);
        }
        if (ANKERL_UNORDERED_DENSE_UNLIKELY(m_buckets.empty())) {
            """ + SCAN + """
        }
        return end();"""
    return replace(s, old, new)


out = sys.argv[1]
write(out, "main", src)
write(out, "two", replace(src, "initial_shifts = 64 - 2;", "initial_shifts = 64 - 1;"))
write(out, "A8", small(8, "A"))
write(out, "A16", small(16, "A"))
write(out, "B8", small(8, "B"))
