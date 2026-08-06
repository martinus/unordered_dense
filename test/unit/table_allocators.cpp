#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/id_allocator.h>
#include <app/map_fixtures.h>

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
using pmr_like = test::pmr_like_allocator<value_type>;

// The same, except that select_on_container_copy_construction does what allocator_traits does by
// default and hands back a copy.
using inheriting = test::id_allocator<value_type>;

// One allocator per propagation question, since each is answered separately and the bug in every
// case was answering it for the values but not for the buckets.
using pocca = test::pocca_allocator<value_type>;
using pocma = test::pocma_allocator<value_type>;
using pocs = test::pocs_allocator<value_type>;

// Carries a map type into a generic lambda, which cannot take a type parameter of its own.
template <typename T>
struct tag_of {
    using type = T;
};

} // namespace

// A copy constructor has to ask the allocator whether it wants to come along. It used to hand the
// source's allocator straight to the extended copy constructor, so a map copied out of an arena
// silently kept that arena alive and kept allocating into it.
TEST_CASE("table_copy_construction_asks_the_allocator") {
    SUBCASE("an allocator that declines is not inherited") {
        auto source = test::filled<map_of<pmr_like>>(20, pmr_like(7));
        auto copy = source;
        REQUIRE(copy.get_allocator().m_id == 0);
        test::require_holds(copy, 20);
        test::require_holds(source, 20);
    }

    SUBCASE("an allocator that accepts is inherited") {
        auto source = test::filled<map_of<inheriting>>(20, inheriting(7));
        auto copy = source;
        REQUIRE(copy.get_allocator().m_id == 7);
        test::require_holds(copy, 20);
    }

    SUBCASE("the segmented map answers the same way") {
        auto source = test::filled<segmented_map_of<pmr_like>>(20, pmr_like(7));
        auto copy = source;
        REQUIRE(copy.get_allocator().m_id == 0);
        test::require_holds(copy, 20);
    }
}

// Both halves of the container have to come from the allocator that was asked for. m_buckets used
// to fall through to its default member initialiser in these two constructors, so the bucket array
// landed in the default resource while the values went where the caller said.
TEST_CASE("table_constructors_put_the_buckets_where_they_were_told") {
    SUBCASE("copy with an allocator") {
        auto source = test::filled<map_of<inheriting>>(200, inheriting(1));
        auto counts = test::alloc_counts{};

        auto copy = map_of<inheriting>(source, inheriting(9, &counts));

        REQUIRE(copy.get_allocator().m_id == 9);
        test::require_holds(copy, 200);
        // Values and buckets, not just values.
        REQUIRE(counts.allocations >= 2);
    }

    SUBCASE("move with an allocator") {
        auto source = test::filled<map_of<inheriting>>(200, inheriting(1));
        auto counts = test::alloc_counts{};

        auto moved = map_of<inheriting>(std::move(source), inheriting(9, &counts));

        test::require_holds(moved, 200);
        REQUIRE(counts.allocations >= 2);
    }

    SUBCASE("the segmented map answers the same way") {
        auto source = test::filled<segmented_map_of<inheriting>>(200, inheriting(1));
        auto counts = test::alloc_counts{};

        auto copy = segmented_map_of<inheriting>(source, inheriting(9, &counts));

        REQUIRE(copy.get_allocator().m_id == 9);
        test::require_holds(copy, 200);
        REQUIRE(counts.allocations >= 2);
    }
}

// Whatever the allocator is, everything it handed out has to come back to it.
TEST_CASE("table_gives_every_block_back_to_the_allocator_that_produced_it") {
    auto counts = test::alloc_counts{};
    {
        auto map = test::filled<map_of<inheriting>>(200, inheriting(3, &counts));
        test::require_holds(map, 200);
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
    auto check = [&](auto tag) {
        using map_type = typename decltype(tag)::type;
        auto source = test::filled<map_type>(200, pocca(1, &source_counts));
        auto target = test::filled<map_type>(50, pocca(2, &target_counts));

        target = source;

        REQUIRE(target.get_allocator().m_id == 1);
        test::require_holds(target, 200);

        // Growing the target reallocates both halves, and every byte of it has to come from
        // allocator 1 now. The buckets used to stay behind, so this went to allocator 2.
        auto const before = target_counts.allocations;
        target.reserve(20000);
        REQUIRE(target_counts.allocations == before);
        test::require_holds(target, 200);
    };

    SUBCASE("map") {
        check(tag_of<map_of<pocca>>{});
    }

    SUBCASE("segmented map") {
        check(tag_of<segmented_map_of<pocca>>{});
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
        auto source = test::filled<map_of<pocma>>(200, pocma(1));
        auto moved = map_of<pocma>(std::move(source), pocma(9));
        REQUIRE(moved.get_allocator().m_id == 9);
        test::require_holds(moved, 200);
    }

    SUBCASE("segmented map") {
        auto source = test::filled<segmented_map_of<pocma>>(200, pocma(1));
        auto moved = segmented_map_of<pocma>(std::move(source), pocma(9));
        REQUIRE(moved.get_allocator().m_id == 9);
        test::require_holds(moved, 200);
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
        using alloc = test::pocma_allocator<int>;
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
        auto a = test::filled<map_of<pocs>>(100, pocs(1));
        auto b = test::filled<map_of<pocs>>(30, pocs(2));

        a.swap(b);

        REQUIRE(a.get_allocator().m_id == 2);
        REQUIRE(b.get_allocator().m_id == 1);
        test::require_holds(a, 30);
        test::require_holds(b, 100);
    }

    SUBCASE("segmented map") {
        auto a = test::filled<segmented_map_of<pocs>>(100, pocs(1));
        auto b = test::filled<segmented_map_of<pocs>>(30, pocs(2));

        a.swap(b);

        REQUIRE(a.get_allocator().m_id == 2);
        REQUIRE(b.get_allocator().m_id == 1);
        test::require_holds(a, 30);
        test::require_holds(b, 100);
    }

    // Equal allocators, so nothing propagates and nothing is reallocated either way.
    SUBCASE("swapping costs no allocation") {
        auto counts = test::alloc_counts{};
        auto a = test::filled<segmented_map_of<inheriting>>(100, inheriting(1, &counts));
        auto b = test::filled<segmented_map_of<inheriting>>(30, inheriting(1, &counts));
        auto const before = counts.allocations;

        a.swap(b);

        REQUIRE(counts.allocations == before);
        test::require_holds(a, 30);
        test::require_holds(b, 100);
    }
}

// An allocator that neither propagates nor compares equal makes the extended move constructor move
// the elements one at a time, and that allocates, so it cannot promise noexcept. It used to promise
// it anyway -- around a body that was *this = std::move(other), which #173 had already made able to
// throw -- which is the terminate-instead-of-throw that #173 set out to remove.
using pmr_like_segmented = segmented_map_of<pmr_like>;

static_assert(!std::is_nothrow_constructible_v<pmr_like_segmented, pmr_like_segmented&&, pmr_like>);

// std::allocator propagates on move assignment and is always equal, so nothing above applies to it
// and the default instantiations keep every promise they had.
static_assert(std::is_nothrow_move_constructible_v<ankerl::unordered_dense::map<int, int>>);
static_assert(std::is_nothrow_move_assignable_v<ankerl::unordered_dense::map<int, int>>);
static_assert(std::is_nothrow_move_constructible_v<ankerl::unordered_dense::segmented_map<int, int>>);
static_assert(std::is_nothrow_move_assignable_v<ankerl::unordered_dense::segmented_map<int, int>>);
