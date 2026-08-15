#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

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

template <typename Map>
void check_hit_is_exactly_one(Map& map) {
    auto const& key = map.begin()->first;

    auto range = map.equal_range(key);
    REQUIRE(range.first == map.begin());
    REQUIRE(range.second == std::next(map.begin()));
    REQUIRE(range.second != map.end());

    auto const_range = std::as_const(map).equal_range(key);
    REQUIRE(const_range.first == std::as_const(map).begin());
    REQUIRE(const_range.second == std::next(std::as_const(map).begin()));
    REQUIRE(const_range.second != std::as_const(map).end());
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
    check_hit_is_exactly_one(map);
    check_miss_is_empty(map, 1000);
}

TEST_CASE_SET("equal_range_set", int) {
    auto set = set_t();
    for (int i = 0; i < 100; ++i) {
        set.emplace(i);
    }
    auto const& key = *set.begin();
    auto range = set.equal_range(key);
    REQUIRE(range.first == set.begin());
    REQUIRE(range.second == std::next(set.begin()));

    auto const_range = std::as_const(set).equal_range(key);
    REQUIRE(const_range.first == std::as_const(set).begin());
    REQUIRE(const_range.second == std::next(std::as_const(set).begin()));

    REQUIRE(set.equal_range(1000) == std::pair(set.end(), set.end()));
}

// The transparent overloads are a separate pair of functions with the same body, and nothing was
// calling them with a key that is present and not last. A string_view argument is what picks them:
// an exact std::string would match the non-template overload, which is the one already covered.
namespace {

struct transparent_hash {
    using is_transparent = void;
    using is_avalanching = void;

    [[nodiscard]] auto operator()(std::string_view sv) const noexcept -> uint64_t {
        return ankerl::unordered_dense::hash<std::string_view>{}(sv);
    }
};

using transparent_map = ankerl::unordered_dense::map<std::string, int, transparent_hash, std::equal_to<>>;

} // namespace

TEST_CASE("equal_range_transparent") {
    auto map = transparent_map();
    for (int i = 0; i < 100; ++i) {
        map.try_emplace("key #"s + std::to_string(i), i);
    }

    auto const key = std::string_view(map.begin()->first);
    auto range = map.equal_range(key);
    REQUIRE(range.first == map.begin());
    REQUIRE(range.second == std::next(map.begin()));
    REQUIRE(range.second != map.end());

    auto const_range = std::as_const(map).equal_range(key);
    REQUIRE(const_range.first == std::as_const(map).begin());
    REQUIRE(const_range.second == std::next(std::as_const(map).begin()));
    REQUIRE(const_range.second != std::as_const(map).end());

    check_miss_is_empty(map, "not in here"sv);

    // And the same overloads reached through count/contains, whose answer for a key that is not
    // there is the one a `? 0 : 1` mutation gets wrong.
    REQUIRE(map.count(key) == 1);
    REQUIRE(map.count("not in here"sv) == 0);
    REQUIRE(map.contains(key));
    REQUIRE(!map.contains("not in here"sv));
}
