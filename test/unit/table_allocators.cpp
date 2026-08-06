#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/id_allocator.h>

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

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

// One allocator per propagation question, since each is answered separately and the bug in every
// case was answering it for the values but not for the buckets.
using pocca = test::id_allocator<value_type, std::true_type>;
using pocma = test::id_allocator<value_type, std::false_type, std::true_type, std::true_type>;
using pocs = test::id_allocator<value_type, std::false_type, std::true_type, std::false_type, std::true_type>;

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

// Item 3 of #174. m_values honours pocca on its own -- that is std::vector's job -- and the
// buckets used to be left behind on the old allocator, so the container advertised one allocator
// while half its memory belonged to another.
TEST_CASE("table_copy_assignment_takes_the_allocator_for_both_halves") {
    auto source_counts = test::alloc_counts{};
    auto target_counts = test::alloc_counts{};

    // Deliberately not an assertion about how many allocations either allocator has served at this
    // point. Which buffers a container is still sitting on midway through a copy assignment is up
    // to the standard library -- libstdc++ and libc++ disagree about it -- and none of that is
    // what this is testing. What has to be true is that allocator 2 is finished with: the target
    // holds nothing of its any more, so nothing the target does from here can reach it.
    auto check = [&](auto make) {
        auto source = make(pocca(1, &source_counts), 200);
        auto target = make(pocca(2, &target_counts), 50);

        target = source;

        REQUIRE(target.get_allocator().m_id == 1);
        require_holds(target, 200);

        // Growing the target reallocates both halves, and every byte of it has to come from
        // allocator 1 now. The buckets used to stay behind, so this went to allocator 2.
        auto const before = target_counts.allocations;
        target.reserve(20000);
        REQUIRE(target_counts.allocations == before);
        require_holds(target, 200);
    };

    SUBCASE("map") {
        check([](pocca alloc, int count) {
            return filled<map_of<pocca>>(alloc, count);
        });
    }

    SUBCASE("segmented map") {
        check([](pocca alloc, int count) {
            return filled<segmented_map_of<pocca>>(alloc, count);
        });
    }

    // ... and once everything is destroyed, both allocators are square. This is the part that
    // would catch a buffer stranded on allocator 2 rather than handed back to it.
    REQUIRE(target_counts.allocations > 0);
    REQUIRE(target_counts.deallocations == target_counts.allocations);
    REQUIRE(source_counts.allocations > 0);
    REQUIRE(source_counts.deallocations == source_counts.allocations);
}

// Item 4 of #174. An extended move constructor has to use the allocator it was handed, whatever
// the allocator says about propagation. This one used to construct empty and then move-assign,
// and assignment asks propagate_on_container_move_assignment instead -- so with a propagating
// allocator the result held the source's allocator and the caller's was dropped.
TEST_CASE("extended_move_construction_uses_the_allocator_it_was_given") {
    SUBCASE("map") {
        auto source = filled<map_of<pocma>>(pocma(1), 200);
        auto moved = map_of<pocma>(std::move(source), pocma(9));
        REQUIRE(moved.get_allocator().m_id == 9);
        require_holds(moved, 200);
    }

    SUBCASE("segmented map") {
        auto source = filled<segmented_map_of<pocma>>(pocma(1), 200);
        auto moved = segmented_map_of<pocma>(std::move(source), pocma(9));
        REQUIRE(moved.get_allocator().m_id == 9);
        require_holds(moved, 200);
    }

    SUBCASE("segmented_vector on its own") {
        using vec = ankerl::unordered_dense::
            segmented_vector<int, test::id_allocator<int, std::false_type, std::true_type, std::true_type>>;
        using alloc = typename vec::allocator_type;

        auto source = vec(alloc(1));
        for (int i = 0; i < 100; ++i) {
            source.emplace_back(i);
        }

        auto moved = vec(std::move(source), alloc(9));

        REQUIRE(moved.get_allocator().m_id == 9);
        REQUIRE(moved.size() == 100);
        for (int i = 0; i < 100; ++i) {
            REQUIRE(moved[static_cast<std::size_t>(i)] == i);
        }
    }

    // What std::vector answers, which is what the above is measured against.
    SUBCASE("std::vector agrees") {
        using alloc = test::id_allocator<int, std::false_type, std::true_type, std::true_type>;
        auto source = std::vector<int, alloc>(alloc(1));
        source.push_back(1);

        auto moved = std::vector<int, alloc>(std::move(source), alloc(9));

        REQUIRE(moved.get_allocator().m_id == 9);
    }
}

// Item 6 of #174. swap has to exchange the allocators when propagate_on_container_swap says so.
// segmented_vector had no member swap, so an unqualified swap() found the generic std::swap, which
// is three moves -- and a move asks about move assignment, not about swap. The two container
// choices answered the same question differently for the same map.
TEST_CASE("swap_exchanges_the_allocators_when_asked_to") {
    SUBCASE("map") {
        auto a = filled<map_of<pocs>>(pocs(1), 100);
        auto b = filled<map_of<pocs>>(pocs(2), 30);

        a.swap(b);

        REQUIRE(a.get_allocator().m_id == 2);
        REQUIRE(b.get_allocator().m_id == 1);
        require_holds(a, 30);
        require_holds(b, 100);
    }

    SUBCASE("segmented map") {
        auto a = filled<segmented_map_of<pocs>>(pocs(1), 100);
        auto b = filled<segmented_map_of<pocs>>(pocs(2), 30);

        a.swap(b);

        REQUIRE(a.get_allocator().m_id == 2);
        REQUIRE(b.get_allocator().m_id == 1);
        require_holds(a, 30);
        require_holds(b, 100);
    }

    // Equal allocators, so nothing propagates and nothing is reallocated either way.
    SUBCASE("swapping costs no allocation") {
        auto counts = test::alloc_counts{};
        auto a = filled<segmented_map_of<inheriting>>(inheriting(1, &counts), 100);
        auto b = filled<segmented_map_of<inheriting>>(inheriting(1, &counts), 30);
        auto const before = counts.allocations;

        a.swap(b);

        REQUIRE(counts.allocations == before);
        require_holds(a, 30);
        require_holds(b, 100);
    }
}

// The member swap is what table::swap calls; without it the call fell back to three moves.
static_assert(std::is_void_v<decltype(std::declval<ankerl::unordered_dense::segmented_vector<int>&>().swap(
                  std::declval<ankerl::unordered_dense::segmented_vector<int>&>()))>);

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
