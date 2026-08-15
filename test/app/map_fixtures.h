#pragma once

#include <app/doctest.h>

#include <cstddef>
#include <utility>

// Fixtures shared by the allocator and exception-safety tests. They all build a map of i -> i and
// then ask the same questions about it, and each of them used to carry its own copy of these --
// four files, four subtly different answers to "does this map hold what it should".
//
// Nothing here is specific to allocators; test/app/id_allocator.h is where that lives.
namespace test {

// A map of count entries, i mapped to i. Built through the (bucket_count, allocator) constructor
// with a count of zero, which is what a default construction resolves to, so the allocator stays
// optional and the buckets are left to be allocated by the first insert.
template <typename Map>
auto filled(int count, typename Map::allocator_type alloc = {}) -> Map {
    auto map = Map(0, alloc);
    for (int i = 0; i < count; ++i) {
        map.try_emplace(i, typename Map::mapped_type(i));
    }
    return map;
}

// ... and it holds exactly that, by lookup rather than by size alone.
template <typename Map>
void require_holds(Map const& map, int count) {
    REQUIRE(map.size() == static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        auto it = map.find(i);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i);
    }
}

// Everything a table with no bucket array has to be able to answer. Two very different routes lead
// to that state -- a table that was default constructed and never inserted into, and one that just
// failed an assignment -- and it is the same state, so both are held to the same list. Keeping one
// list is the point: the post-throw table is the harder of the two to get right, and it used to be
// checked against the shorter of two lists.
template <typename Map>
void require_empty_table_answers(Map& map) {
    REQUIRE(map.empty());
    REQUIRE(map.size() == 0);
    REQUIRE(map.bucket_count() == 0);
    REQUIRE(map.load_factor() == 0.0F);

    REQUIRE(map.begin() == map.end());
    REQUIRE(map.find(1) == map.end());
    REQUIRE(std::as_const(map).find(1) == map.end());
    REQUIRE(map.count(1) == 0);
    REQUIRE(!map.contains(1));
    REQUIRE(map.equal_range(1).first == map.end());
    REQUIRE(map.erase(1) == 0);

    map.clear();
    REQUIRE(map.bucket_count() == 0);

    // ... and it is still a working map, which is the part that says the state is valid rather
    // than merely quiet.
    map.try_emplace(7, typename Map::mapped_type(7));
    REQUIRE(map.size() == 1);
    REQUIRE(map.find(7) != map.end());

    // And the array that insert just allocated is the size a fresh table's first insert allocates,
    // not the size this table had grown to before it was emptied. The shift that decides it is the
    // one piece of the state with no getter, so this is the only place it shows: a table that kept
    // it answers everything above identically and then allocates the whole of its old array for one
    // element. Every route into this state -- moved from, assignment that threw, a copy of an
    // emptied table -- has been wrong here at some point.
    auto fresh = Map();
    fresh.try_emplace(7, typename Map::mapped_type(7));
    REQUIRE(map.bucket_count() == fresh.bucket_count());

    // The load factor is part of "looks default constructed" too, and it is carried separately
    // from everything else.
    REQUIRE(map.max_load_factor() == fresh.max_load_factor());
}

} // namespace test
