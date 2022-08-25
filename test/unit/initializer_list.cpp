#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <cstddef>     // for size_t
#include <string>      // for string, operator==, allocator
#include <string_view> // for basic_string_view, operator""sv
#include <utility>     // for pair
#include <vector>      // for vector

using namespace std::literals;

template <class Map, class First, class Second>
auto has(Map const& map, First const& first, Second const& second) -> bool {
    auto it = map.find(first);
    return it != map.end() && it->second == second;
}

TEST_CASE("insert_initializer_list") {
    auto m = ankerl::unordered_dense::map<int, int>();
    m.insert({{1, 2}, {3, 4}, {5, 6}});
    REQUIRE(m.size() == 3U);
    REQUIRE(m[1] == 2);
    REQUIRE(m[3] == 4);
    REQUIRE(m[5] == 6);
}

TEST_CASE("initializerlist_string") {
    size_t const n1 = 17;
    size_t const n2 = 10;

    auto m1 = ankerl::unordered_dense::map<std::string, size_t>{{"duck", n1}, {"lion", n2}};
    auto m2 = ankerl::unordered_dense::map<std::string, size_t>{{"duck", n1}, {"lion", n2}};

    REQUIRE(m1.size() == 2);
    REQUIRE(m1["duck"] == n1);
    REQUIRE(m1["lion"] == n2);

    REQUIRE(m2.size() == 2);
    auto it = m2.find("duck");
    REQUIRE((it != m2.end() && it->second == n1));
    REQUIRE(m2["lion"] == n2);
}

TEST_CASE("insert_initializer_list_string") {
    auto m = ankerl::unordered_dense::map<int, std::string>();
    m.insert({{1, "a"}, {3, "b"}, {5, "c"}});
    REQUIRE(m.size() == 3U);
    REQUIRE(m[1] == "a");
    REQUIRE(m[3] == "b");
    REQUIRE(m[5] == "c");
}

TEST_CASE("initializer_list_assign") {
    auto map = ankerl::unordered_dense::map<int, char const*>();
    map[3] = "nope";
    map = {{1, "a"}, {2, "hello"}, {12346, "world!"}};
    REQUIRE(map.size() == 3);
    REQUIRE(has(map, 1, "a"sv));
    REQUIRE(has(map, 2, "hello"sv));
    REQUIRE(has(map, 12346, "world!"sv));
}

TEST_CASE("initializer_list_ctor_alloc") {
    using alloc_t = std::allocator<std::pair<int, char const*>>;
    auto map = ankerl::unordered_dense::map<int, char const*>({{1, "a"}, {2, "hello"}, {12346, "world!"}}, 0, alloc_t{});
    REQUIRE(map.size() == 3);
    REQUIRE(has(map, 1, "a"sv));
    REQUIRE(has(map, 2, "hello"sv));
    REQUIRE(has(map, 12346, "world!"sv));
}

TEST_CASE("initializer_list_ctor_hash_alloc") {
    using hash_t = ankerl::unordered_dense::hash<int>;
    using alloc_t = std::allocator<std::pair<int, char const*>>;
    auto map =
        ankerl::unordered_dense::map<int, char const*>({{1, "a"}, {2, "hello"}, {12346, "world!"}}, 0, hash_t{}, alloc_t{});
    REQUIRE(map.size() == 3);
    REQUIRE(has(map, 1, "a"sv));
    REQUIRE(has(map, 2, "hello"sv));
    REQUIRE(has(map, 12346, "world!"sv));
}
