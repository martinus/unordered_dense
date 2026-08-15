#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/id_allocator.h>
#include <app/map_fixtures.h>

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

// An empty container whose allocator reports back. Built through the (bucket_count, allocator)
// constructor with a count of zero, which is what a default construction resolves to. Not
// test::filled, which needs a mapped_type and so cannot serve the set cases below.
template <typename Container>
auto tracked(test::alloc_counts& counts) -> Container {
    return Container(0, typename Container::allocator_type(0, &counts));
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
// early on empty(); this pins that they actually do. The list itself is shared with the test for a
// table that just failed an assignment, which lands in the same state by a different route.
TEST_CASE("a_table_without_buckets_answers_every_query") {
    auto map = ankerl::unordered_dense::map<int, int>();
    test::require_empty_table_answers(map);

    auto swapped = ankerl::unordered_dense::map<int, int>();
    auto other = ankerl::unordered_dense::map<int, int>();
    swapped.swap(other);
    REQUIRE(swapped.empty());

    // extract() clears the buckets on the way out, with none to clear.
    auto extracted = ankerl::unordered_dense::map<int, int>();
    REQUIRE(std::move(extracted).extract().empty());
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
        test::require_holds(target, 100);
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
    test::require_holds(map, count);

    for (int i = 0; i < count; ++i) {
        REQUIRE(map.erase(i) == 1);
    }
    REQUIRE(map.empty());
    REQUIRE(map.bucket_count() > 0); // erasing does not give the buckets back

    // Refilling reuses them.
    for (int i = 0; i < count; ++i) {
        map[i] = i;
    }
    test::require_holds(map, count);
}

// Two tables in different states, swapped both ways round.
TEST_CASE("swapping_an_unallocated_table_with_a_full_one") {
    auto empty = ankerl::unordered_dense::map<int, int>();
    auto full = ankerl::unordered_dense::map<int, int>();
    for (int i = 0; i < 100; ++i) {
        full[i] = i;
    }

    empty.swap(full);

    test::require_holds(empty, 100);
    REQUIRE(full.empty());
    REQUIRE(full.bucket_count() == 0);

    full[1] = 2;
    REQUIRE(full.find(1)->second == 2);
}

// The three routes to "empty, and looks like it was always empty". Answering every query is the
// easy half, and the one already covered above; the hard half is the shift, which nothing exposes
// and which decides how big an array the next insert asks for. A table that kept the shift of the
// table it used to be allocates that whole array again for one element.
TEST_CASE_MAP("an_emptied_table_looks_default_constructed", int, int) {
    SUBCASE("moved from") {
        auto source = map_t();
        for (int i = 0; i < 500; ++i) {
            source[i] = i;
        }
        auto const sink = std::move(source);
        REQUIRE(sink.size() == 500);

        // Using a moved-from object is exactly what is being tested: the standard requires it to be
        // valid, and this table promises more than that -- it promises to be indistinguishable from
        // a fresh one.
        // NOLINTNEXTLINE(bugprone-use-after-move,hicpp-invalid-access-moved,clang-analyzer-cplusplus.Move)
        test::require_empty_table_answers(source);
    }

    SUBCASE("a copy of a table that was cleared") {
        auto source = map_t();
        for (int i = 0; i < 500; ++i) {
            source[i] = i;
        }
        source.clear();
        auto copy = source;
        test::require_empty_table_answers(copy);
    }

    SUBCASE("assigned from a table that was cleared") {
        auto source = map_t();
        for (int i = 0; i < 500; ++i) {
            source[i] = i;
        }
        source.clear();

        auto target = map_t();
        for (int i = 0; i < 20; ++i) {
            target[i] = i;
        }
        target = source;
        test::require_empty_table_answers(target);
    }
}

// max_load_factor is carried by hand in the copy constructor and by hand again in the move, and it
// is the one piece of configuration a table has. A copy that quietly reverts to the default grows
// at a different point than the table it was copied from.
TEST_CASE_MAP("a_copy_keeps_the_max_load_factor", int, int) {
    auto source = map_t();
    source.max_load_factor(0.5F);
    for (int i = 0; i < 100; ++i) {
        source[i] = i;
    }
    REQUIRE(source.max_load_factor() == 0.5F);

    auto const copy = source;
    REQUIRE(copy.max_load_factor() == 0.5F);
    // Same configuration, same number of buckets for the same elements.
    REQUIRE(copy.bucket_count() == source.bucket_count());

    auto assigned = map_t();
    assigned[1] = 1;
    assigned = source;
    REQUIRE(assigned.max_load_factor() == 0.5F);
    REQUIRE(assigned.bucket_count() == source.bucket_count());

    auto const source_buckets = source.bucket_count();
    auto const moved = std::move(source);
    REQUIRE(moved.max_load_factor() == 0.5F);
    REQUIRE(moved.bucket_count() == source_buckets);
}

// reserve() is a promise about the buckets as much as about the values: after it, inserting that
// many elements must not rehash. Only the bucket count can see the difference -- the answers are
// right either way, the table just does the growing it was asked to do in advance.
TEST_CASE_MAP("reserve_sizes_the_buckets_not_just_the_values", int, int) {
    auto map = map_t();
    map[1] = 1; // so the table already has buckets, which is the case that regressed

    map.reserve(1000);
    auto const after_reserve = map.bucket_count();
    REQUIRE(after_reserve >= 1000);

    for (int i = 0; i < 1000; ++i) {
        map[i] = i;
    }
    REQUIRE(map.bucket_count() == after_reserve);
    REQUIRE(map.size() == 1000);
}
