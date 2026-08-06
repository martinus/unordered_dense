#include <ankerl/unordered_dense.h>

#include <app/bombing_allocator.h>
#include <app/doctest.h>

#include <cstddef>
#include <functional>
#include <new>
#include <utility>

// Issue #182. reserve, rehash, replace and growing on insert all need a bigger bucket array, and
// all of them used to give the old one back before asking for the new one. When the ask failed the
// table was left holding values with no buckets to find them by, which no operation is prepared
// for: do_find returns early only while m_values is empty, so a non-empty table with no buckets
// probed a container of length zero and crashed.
//
// Each of them now builds the new array beside the old one, so a failure leaves the table exactly
// as it was.
namespace {

using pair_t = std::pair<int, int>;

template <typename Map>
auto filled(int count) -> Map {
    auto map = Map();
    for (int i = 0; i < count; ++i) {
        map[i] = i;
    }
    return map;
}

// The table has to agree with itself: everything size() counts has to be reachable, and everything
// reachable has to have the value it was given.
template <typename Map>
void require_consistent(Map const& map, int searched_up_to) {
    auto found = 0;
    for (int i = 0; i < searched_up_to; ++i) {
        auto it = map.find(i);
        if (it != map.end()) {
            REQUIRE(it->second == i);
            ++found;
        }
    }
    REQUIRE(map.size() == static_cast<std::size_t>(found));

    // Iteration has to see exactly the same elements.
    auto iterated = std::size_t{0};
    for (auto const& entry : map) {
        REQUIRE(map.find(entry.first) != map.end());
        ++iterated;
    }
    REQUIRE(iterated == map.size());
}

// Runs op with every allocation budget up to a bound, and holds the table to the same standard
// after each failure. Which allocation inside op is the bucket one is not worth guessing -- and it
// moves as the code changes -- so every one of them gets its turn.
template <typename Map, typename Op>
void require_survives_every_allocation_failure(int start_size, int searched_up_to, Op op) {
    auto failures = 0;
    for (int budget = 0; budget < 12; ++budget) {
        auto map = filled<Map>(start_size);
        auto const before_size = map.size();

        try {
            auto const bomb = test::bomb_after(budget);
            op(map);
        } catch (std::bad_alloc const&) {
            ++failures;
        }

        require_consistent(map, searched_up_to);
        // Nothing is ever lost: the table either did the whole thing or none of it.
        REQUIRE(map.size() >= before_size);
    }
    // If nothing ever threw the test would be vacuous.
    REQUIRE(failures > 0);
}

template <typename Alloc>
using map_of = ankerl::unordered_dense::map<int, int, ankerl::unordered_dense::hash<int>, std::equal_to<int>, Alloc>;

template <typename Alloc>
using segmented_map_of =
    ankerl::unordered_dense::segmented_map<int, int, ankerl::unordered_dense::hash<int>, std::equal_to<int>, Alloc>;

using bombing_map = map_of<test::bombing_allocator<pair_t>>;
using bombing_segmented_map = segmented_map_of<test::bombing_allocator<pair_t>>;

} // namespace

TEST_CASE("reserve_that_cannot_allocate_leaves_the_table_alone") {
    require_survives_every_allocation_failure<bombing_map>(100, 200, [](bombing_map& map) {
        map.reserve(100000);
    });
}

TEST_CASE("rehash_that_cannot_allocate_leaves_the_table_alone") {
    require_survives_every_allocation_failure<bombing_map>(100, 200, [](bombing_map& map) {
        map.rehash(100000);
    });
}

TEST_CASE("replace_that_cannot_allocate_leaves_the_table_alone") {
    require_survives_every_allocation_failure<bombing_map>(100, 6000, [](bombing_map& map) {
        // Built with the countdown already running, so whichever allocation fails first is the one
        // under test -- including the ones inside replace() itself.
        auto container = bombing_map::value_container_type{};
        for (int i = 0; i < 5000; ++i) {
            container.emplace_back(i, i);
        }
        map.replace(std::move(container));
    });
}

// The one reachable without asking for anything: an ordinary insert loop, growing.
TEST_CASE("growing_on_insert_that_cannot_allocate_leaves_the_table_alone") {
    require_survives_every_allocation_failure<bombing_map>(100, 4000, [](bombing_map& map) {
        for (int i = 100; i < 3000; ++i) {
            map[i] = i;
        }
    });
}

TEST_CASE("the_segmented_map_survives_the_same_failures") {
    require_survives_every_allocation_failure<bombing_segmented_map>(100, 4000, [](bombing_segmented_map& map) {
        for (int i = 100; i < 3000; ++i) {
            map[i] = i;
        }
    });

    require_survives_every_allocation_failure<bombing_segmented_map>(100, 200, [](bombing_segmented_map& map) {
        map.reserve(100000);
    });
}

// Shrinking is the other direction, and it is not in the harness above because it cannot fail:
// it hands storage back rather than asking for it, so there is no allocation to bomb. It needs its
// own case anyway, because the container that grows in place -- the segmented and custom ones --
// has to be told to shrink now that the caller no longer empties it first. Getting that wrong
// leaves the array at its old size, which is what rehash's own test caught.
TEST_CASE_MAP("shrinking_the_bucket_array_keeps_it_consistent", int, int) {
    auto map = filled<map_t>(2000);

    map.rehash(100000);
    auto const grown = map.bucket_count();
    REQUIRE(grown >= 100000);

    map.rehash(0);
    REQUIRE(map.bucket_count() < grown);
    REQUIRE(map.bucket_count() >= map.size());
    require_consistent(map, 3000);
}
