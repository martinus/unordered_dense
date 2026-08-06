#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/id_allocator.h>

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

// Allocator handling in detail::table, from issue #174. The container-level counterpart to the
// segmented_vector fixes in #173 -- the same decisions are made again one level up, over m_values
// *and* m_buckets, and the map is what users actually hold.
namespace {

using value_type = std::pair<int, int>;

template <typename Alloc>
using map_of = ankerl::unordered_dense::map<int, int, ankerl::unordered_dense::hash<int>, std::equal_to<int>, Alloc>;

template <typename Alloc>
using segmented_map_of =
    ankerl::unordered_dense::segmented_map<int, int, ankerl::unordered_dense::hash<int>, std::equal_to<int>, Alloc>;

// Propagates on nothing, instances differ, and a copy does not inherit the allocator -- the shape
// of std::pmr::polymorphic_allocator, which is the allocator whose propagation this library has to
// get right.
using pmr_like = test::id_allocator<value_type, std::false_type, std::false_type>;

// The same, except that select_on_container_copy_construction does what allocator_traits does by
// default and hands back a copy.
using inheriting = test::id_allocator<value_type>;

template <typename Map>
auto filled(typename Map::allocator_type alloc, int count) -> Map {
    auto map = Map(0, alloc);
    for (int i = 0; i < count; ++i) {
        map[i] = i;
    }
    return map;
}

template <typename Map>
void require_holds(Map const& map, int count) {
    REQUIRE(map.size() == static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        auto it = map.find(i);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i);
    }
}

} // namespace

// A copy constructor has to ask the allocator whether it wants to come along. It used to hand the
// source's allocator straight to the extended copy constructor, so a map copied out of an arena
// silently kept that arena alive and kept allocating into it.
TEST_CASE("table_copy_construction_asks_the_allocator") {
    SUBCASE("an allocator that declines is not inherited") {
        auto source = filled<map_of<pmr_like>>(pmr_like(7), 20);
        auto copy = source;
        REQUIRE(copy.get_allocator().m_id == 0);
        require_holds(copy, 20);
        require_holds(source, 20);
    }

    SUBCASE("an allocator that accepts is inherited") {
        auto source = filled<map_of<inheriting>>(inheriting(7), 20);
        auto copy = source;
        REQUIRE(copy.get_allocator().m_id == 7);
        require_holds(copy, 20);
    }

    SUBCASE("the segmented map answers the same way") {
        auto source = filled<segmented_map_of<pmr_like>>(pmr_like(7), 20);
        auto copy = source;
        REQUIRE(copy.get_allocator().m_id == 0);
        require_holds(copy, 20);
    }
}

// Both halves of the container have to come from the allocator that was asked for. m_buckets used
// to fall through to its default member initialiser in these two constructors, so the bucket array
// landed in the default resource while the values went where the caller said.
TEST_CASE("table_constructors_put_the_buckets_where_they_were_told") {
    SUBCASE("copy with an allocator") {
        auto source = filled<map_of<inheriting>>(inheriting(1), 200);
        auto counts = test::alloc_counts{};

        auto copy = map_of<inheriting>(source, inheriting(9, &counts));

        REQUIRE(copy.get_allocator().m_id == 9);
        require_holds(copy, 200);
        // Values and buckets, not just values.
        REQUIRE(counts.allocations >= 2);
    }

    SUBCASE("move with an allocator") {
        auto source = filled<map_of<inheriting>>(inheriting(1), 200);
        auto counts = test::alloc_counts{};

        auto moved = map_of<inheriting>(std::move(source), inheriting(9, &counts));

        require_holds(moved, 200);
        REQUIRE(counts.allocations >= 2);
    }

    SUBCASE("the segmented map answers the same way") {
        auto source = filled<segmented_map_of<inheriting>>(inheriting(1), 200);
        auto counts = test::alloc_counts{};

        auto copy = segmented_map_of<inheriting>(source, inheriting(9, &counts));

        REQUIRE(copy.get_allocator().m_id == 9);
        require_holds(copy, 200);
        REQUIRE(counts.allocations >= 2);
    }
}

// Whatever the allocator is, everything it handed out has to come back to it.
TEST_CASE("table_gives_every_block_back_to_the_allocator_that_produced_it") {
    auto counts = test::alloc_counts{};
    {
        auto map = filled<map_of<inheriting>>(inheriting(3, &counts), 200);
        require_holds(map, 200);
    }
    REQUIRE(counts.allocations > 0);
    REQUIRE(counts.deallocations == counts.allocations);
}

// The extended move constructor's body is *this = std::move(other), which for an allocator that
// neither propagates nor compares equal moves the elements one at a time and allocates. #173 made
// that assignment able to throw; the constructor around it still promised it could not, which is
// the terminate-instead-of-throw that #173 set out to remove.
using pmr_like_map = map_of<pmr_like>;
using pmr_like_segmented = segmented_map_of<pmr_like>;

static_assert(!std::is_nothrow_constructible_v<pmr_like_segmented, pmr_like_segmented&&, pmr_like>);

// std::allocator propagates on move assignment and is always equal, so nothing above applies to it
// and the default instantiations keep every promise they had.
static_assert(std::is_nothrow_move_constructible_v<ankerl::unordered_dense::map<int, int>>);
static_assert(std::is_nothrow_move_assignable_v<ankerl::unordered_dense::map<int, int>>);
static_assert(std::is_nothrow_move_constructible_v<ankerl::unordered_dense::segmented_map<int, int>>);
static_assert(std::is_nothrow_move_assignable_v<ankerl::unordered_dense::segmented_map<int, int>>);
