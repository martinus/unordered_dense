#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/id_allocator.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Issue #159: default construction used to allocate the bucket array, so a map declared in a scope
// that might never use it still cost an allocation. It is now allocated by the first insert, which
// means "no elements" and "no buckets" are the same state -- and every operation has to keep
// working in it.
namespace {

using pair_t = std::pair<int, int>;

template <typename Alloc>
using map_of = ankerl::unordered_dense::map<int, int, ankerl::unordered_dense::hash<int>, std::equal_to<int>, Alloc>;

template <typename Alloc>
using segmented_map_of =
    ankerl::unordered_dense::segmented_map<int, int, ankerl::unordered_dense::hash<int>, std::equal_to<int>, Alloc>;

template <typename Alloc>
using set_of = ankerl::unordered_dense::set<int, ankerl::unordered_dense::hash<int>, std::equal_to<int>, Alloc>;

using counting_alloc = test::id_allocator<pair_t>;
using counting_map = map_of<counting_alloc>;
using counting_segmented_map = segmented_map_of<counting_alloc>;
using counting_set = set_of<test::id_allocator<int>>;

// A set whose emplace() goes through the single-argument transparent overload rather than the
// variadic one -- a third insert entry point, with its own path to the buckets.
struct transparent_hash {
    using is_transparent = void;
    using is_avalanching = void;

    auto operator()(std::string_view sv) const noexcept -> std::uint64_t {
        return ankerl::unordered_dense::hash<std::string_view>{}(sv);
    }
};

using transparent_set =
    ankerl::unordered_dense::set<std::string, transparent_hash, std::equal_to<>, test::id_allocator<std::string>>;

// Every container here is built through the (bucket_count, allocator) constructor with a count of
// zero, which is what a default construction resolves to.
template <typename Map>
auto tracked(test::alloc_counts& counts) -> Map {
    return Map(0, typename Map::allocator_type(0, &counts));
}

// What a table costs before it has allocated anything of its own. Not zero everywhere: a table is
// two containers, the values and the buckets, and MSVC's debug iterator support allocates a
// _Container_proxy through the allocator for each one as it is constructed. So the floor is
// measured rather than assumed -- two empty vectors' worth, whatever that is here.
auto empty_table_cost() -> int {
    auto counts = test::alloc_counts{};
    {
        auto values = std::vector<pair_t, test::id_allocator<pair_t>>(test::id_allocator<pair_t>(0, &counts));
        auto buckets = std::vector<pair_t, test::id_allocator<pair_t>>(test::id_allocator<pair_t>(0, &counts));
        static_cast<void>(values);
        static_cast<void>(buckets);
    }
    return counts.allocations;
}

template <typename Map>
void require_holds(Map const& map, int count) {
    REQUIRE(map.size() == static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        REQUIRE(map.find(i) != map.end());
    }
}

} // namespace

TEST_CASE("default_construction_allocates_nothing") {
    auto counts = test::alloc_counts{};

    SUBCASE("map") {
        auto map = tracked<counting_map>(counts);
        REQUIRE(counts.allocations == empty_table_cost());
        REQUIRE(map.bucket_count() == 0);
        REQUIRE(map.size() == 0);
        REQUIRE(map.empty());
    }

    SUBCASE("segmented map") {
        auto map = tracked<counting_segmented_map>(counts);
        REQUIRE(counts.allocations == empty_table_cost());
        REQUIRE(map.bucket_count() == 0);
    }

    SUBCASE("set") {
        auto set = tracked<counting_set>(counts);
        REQUIRE(counts.allocations == empty_table_cost());
        REQUIRE(set.bucket_count() == 0);
    }
}

// Nothing may reach the bucket array while it is not there. The read paths get this by returning
// early on empty(); this pins that they actually do.
TEST_CASE("a_table_without_buckets_answers_every_query") {
    auto map = ankerl::unordered_dense::map<int, int>();

    REQUIRE(map.find(1) == map.end());
    REQUIRE(map.count(1) == 0);
    REQUIRE(!map.contains(1));
    REQUIRE(map.erase(1) == 0);
    REQUIRE(map.begin() == map.end());
    REQUIRE(map.load_factor() == 0.0F);
    REQUIRE(map.equal_range(1).first == map.end());
    REQUIRE(std::as_const(map).find(1) == map.end());

    auto other = ankerl::unordered_dense::map<int, int>();
    map.swap(other);
    REQUIRE(map.empty());

    map.clear();
    REQUIRE(map.bucket_count() == 0);

    // extract() clears the buckets on the way out, with none to clear.
    REQUIRE(std::move(map).extract().empty());
}

// One case per insert entry point, each starting from a container that has no buckets.
TEST_CASE("the_first_insert_allocates_the_buckets") {
    auto counts = test::alloc_counts{};

    SUBCASE("operator[]") {
        auto map = tracked<counting_map>(counts);
        map[1] = 2;
        REQUIRE(map.bucket_count() > 0);
        REQUIRE(map[1] == 2);
        REQUIRE(counts.allocations >= 2); // values and buckets
    }

    SUBCASE("try_emplace") {
        auto map = tracked<counting_map>(counts);
        REQUIRE(map.try_emplace(1, 2).second);
        REQUIRE(map.find(1)->second == 2);
    }

    SUBCASE("insert") {
        auto map = tracked<counting_map>(counts);
        REQUIRE(map.insert(pair_t(1, 2)).second);
        REQUIRE(map.find(1)->second == 2);
    }

    SUBCASE("emplace") {
        auto map = tracked<counting_map>(counts);
        REQUIRE(map.emplace(1, 2).second);
        REQUIRE(map.find(1)->second == 2);
    }

    SUBCASE("insert_or_assign") {
        auto map = tracked<counting_map>(counts);
        REQUIRE(map.insert_or_assign(1, 2).second);
        REQUIRE(map.find(1)->second == 2);
    }

    SUBCASE("range insert") {
        auto source = std::vector<pair_t>{{1, 2}, {3, 4}};
        auto map = tracked<counting_map>(counts);
        map.insert(source.begin(), source.end());
        REQUIRE(map.size() == 2);
    }

    SUBCASE("initializer list insert") {
        auto map = tracked<counting_map>(counts);
        map.insert({{1, 2}, {3, 4}});
        REQUIRE(map.size() == 2);
    }

    SUBCASE("set emplace") {
        auto set = tracked<counting_set>(counts);
        REQUIRE(set.emplace(1).second);
        REQUIRE(set.contains(1));
    }

    // The single-argument transparent overload, which probes the buckets before constructing
    // anything at all.
    SUBCASE("transparent set emplace") {
        auto set = transparent_set(0, test::id_allocator<std::string>(0, &counts));
        REQUIRE(counts.allocations == empty_table_cost());
        REQUIRE(set.emplace(std::string_view("hello")).second);
        REQUIRE(set.contains(std::string_view("hello")));
    }

    SUBCASE("replace") {
        auto map = tracked<counting_map>(counts);
        map.replace(counting_map::value_container_type{{1, 2}, {3, 4}});
        REQUIRE(map.size() == 2);
        REQUIRE(map.find(3)->second == 4);
    }

    SUBCASE("segmented map") {
        auto map = tracked<counting_segmented_map>(counts);
        map[1] = 2;
        REQUIRE(map.bucket_count() > 0);
        REQUIRE(map[1] == 2);
    }
}

TEST_CASE("copying_an_empty_table_allocates_nothing") {
    auto counts = test::alloc_counts{};
    auto source = tracked<counting_map>(counts);

    auto copy = source;
    REQUIRE(counts.allocations == 2 * empty_table_cost()); // the source and the copy, nothing else
    REQUIRE(copy.bucket_count() == 0);

    auto assigned = tracked<counting_map>(counts);
    assigned[1] = 2;
    assigned = source;
    REQUIRE(assigned.empty());
    REQUIRE(assigned.bucket_count() == 0);

    // ... and it is still a working map afterwards.
    assigned[7] = 8;
    REQUIRE(assigned.find(7)->second == 8);
}

// A moved-from table used to be handed a freshly allocated bucket array so that it stayed usable.
// It is usable without one now, so the move does not allocate.
TEST_CASE("a_moved_from_table_keeps_no_buckets") {
    auto counts = test::alloc_counts{};

    SUBCASE("move assignment") {
        auto source = tracked<counting_map>(counts);
        for (int i = 0; i < 100; ++i) {
            source[i] = i;
        }
        auto target = tracked<counting_map>(counts);
        auto const before = counts.allocations;

        target = std::move(source);

        REQUIRE(counts.allocations == before);
        require_holds(target, 100);
        REQUIRE(source.bucket_count() == 0); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        REQUIRE(source.empty());             // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)

        source[1] = 2; // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        REQUIRE(source.find(1)->second == 2);
    }

    SUBCASE("move construction") {
        auto source = tracked<counting_map>(counts);
        source[1] = 2;
        auto const before = counts.allocations;

        auto target = std::move(source);

        // Nothing beyond bringing the target's own two containers into existence.
        REQUIRE(counts.allocations == before + empty_table_cost());
        REQUIRE(target.find(1)->second == 2);
        REQUIRE(source.bucket_count() == 0); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    }
}

// An explicit request is still honoured up front -- laziness applies to the count that was not
// asked for, not to the one that was.
TEST_CASE("an_explicit_bucket_count_is_still_allocated_up_front") {
    auto counts = test::alloc_counts{};

    auto map = counting_map(150U, counting_alloc(0, &counts));
    REQUIRE(map.bucket_count() == 256U);
    REQUIRE(counts.allocations > 0);

    auto reserved = tracked<counting_map>(counts);
    reserved.reserve(1000);
    REQUIRE(reserved.bucket_count() >= 1000);
}

// The lazily allocated table has to behave like the eagerly allocated one did, including on the
// paths that grow and shrink it.
TEST_CASE("a_lazily_allocated_table_grows_and_empties_like_any_other") {
    auto map = ankerl::unordered_dense::map<int, int>();
    auto const count = 1000;

    for (int i = 0; i < count; ++i) {
        map[i] = i;
    }
    require_holds(map, count);

    for (int i = 0; i < count; ++i) {
        REQUIRE(map.erase(i) == 1);
    }
    REQUIRE(map.empty());
    REQUIRE(map.bucket_count() > 0); // erasing does not give the buckets back

    // Refilling reuses them.
    for (int i = 0; i < count; ++i) {
        map[i] = i;
    }
    require_holds(map, count);
}

// Two tables in different states, swapped both ways round.
TEST_CASE("swapping_an_unallocated_table_with_a_full_one") {
    auto empty = ankerl::unordered_dense::map<int, int>();
    auto full = ankerl::unordered_dense::map<int, int>();
    for (int i = 0; i < 100; ++i) {
        full[i] = i;
    }

    empty.swap(full);

    require_holds(empty, 100);
    REQUIRE(full.empty());
    REQUIRE(full.bucket_count() == 0);

    full[1] = 2;
    REQUIRE(full.find(1)->second == 2);
}
