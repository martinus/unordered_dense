#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/hashers.h>

#include <cstdint>     // for uint64_t
#include <functional>  // for equal_to
#include <iterator>    // for next
#include <string>      // for string, basic_string
#include <string_view> // for string_view, basic_string_view
#include <type_traits> // for add_const_t
#include <utility>     // for pair, as_const
#include <vector>      // for vector

using namespace std::literals;

TEST_CASE_MAP("equal_range", int, int) {
    auto map = map_t();
    // auto map = std::unordered_map<int, int>();

    auto range = map.equal_range(123);
    REQUIRE(range.first == map.end());
    REQUIRE(range.second == map.end());

    map.try_emplace(1, 1);
    range = map.equal_range(123);
    REQUIRE(range.first == map.end());
    REQUIRE(range.second == map.end());

    int const x = 1;
    auto const_range = std::as_const(map).equal_range(x);
    REQUIRE(const_range.first == map.begin());
    REQUIRE(const_range.second == map.end());

    for (int i = 0; i < 100; ++i) {
        map.try_emplace(i, i);
    }
    range = map.equal_range(50);
    auto after_first = ++range.first;
    REQUIRE(range.second == after_first);
    REQUIRE(range.second != map.end());
}

// The check above is `second == ++first`, which is the contract -- but it was only ever made of the
// non-const overload, and the const one was asked with a single element in the table. With one
// element `it + 1` and `end()` are the same iterator, so a `second` that is always `end()` passes
// it. The key here is the one at begin() instead: with a hundred elements behind it, "one past the
// hit" and "the end" are as far apart as they get.
namespace {

// `key` has to be the one at begin(), which every caller arranges: that is what puts "one past the
// hit" as far from "the end" as the container allows. Taking it as a parameter rather than reading
// it here is what lets a set (where it is *begin(), not begin()->first) and a transparent lookup
// (where it has to be a string_view to select the template overload) use the same six assertions.
template <typename Container, typename Key>
void check_hit_is_exactly_one(Container& container, Key const& key) {
    auto range = container.equal_range(key);
    REQUIRE(range.first == container.begin());
    REQUIRE(range.second == std::next(container.begin()));
    REQUIRE(range.second != container.end());

    auto const_range = std::as_const(container).equal_range(key);
    REQUIRE(const_range.first == std::as_const(container).begin());
    REQUIRE(const_range.second == std::next(std::as_const(container).begin()));
    REQUIRE(const_range.second != std::as_const(container).end());
}

template <typename Map, typename Key>
void check_miss_is_empty(Map& map, Key const& absent) {
    auto range = map.equal_range(absent);
    REQUIRE(range.first == map.end());
    REQUIRE(range.second == map.end());

    auto const_range = std::as_const(map).equal_range(absent);
    REQUIRE(const_range.first == std::as_const(map).end());
    REQUIRE(const_range.second == std::as_const(map).end());
}

} // namespace

TEST_CASE_MAP("equal_range_const_overload_with_more_than_one_element", int, int) {
    auto map = map_t();
    for (int i = 0; i < 100; ++i) {
        map.try_emplace(i, i);
    }
    check_hit_is_exactly_one(map, map.begin()->first);
    check_miss_is_empty(map, 1000);
}

TEST_CASE_SET("equal_range_set", int) {
    auto set = set_t();
    for (int i = 0; i < 100; ++i) {
        set.emplace(i);
    }
    check_hit_is_exactly_one(set, *set.begin());
    check_miss_is_empty(set, 1000);
}

// The transparent overloads are a separate pair of functions with the same body, and nothing was
// calling them with a key that is present and not last. A string_view argument is what picks them:
// an exact std::string would match the non-template overload, which is the one already covered.
namespace {

using transparent_map = ankerl::unordered_dense::map<std::string, int, test::transparent_hash, std::equal_to<>>;

} // namespace

TEST_CASE("equal_range_transparent") {
    auto map = transparent_map();
    for (int i = 0; i < 100; ++i) {
        map.try_emplace("key #"s + std::to_string(i), i);
    }

    auto const key = std::string_view(map.begin()->first);
    check_hit_is_exactly_one(map, key);
    check_miss_is_empty(map, "not in here"sv);

    // And the same overloads reached through count/contains, whose answer for a key that is not
    // there is the one a `? 0 : 1` mutation gets wrong.
    REQUIRE(map.count(key) == 1);
    REQUIRE(map.count("not in here"sv) == 0);
    REQUIRE(map.contains(key));
    REQUIRE(!map.contains("not in here"sv));
}
