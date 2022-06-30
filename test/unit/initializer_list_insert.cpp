#include <ankerl/unordered_dense.h>

#include <doctest.h>

TEST_CASE("insert_initializer_list") {
    auto m = ankerl::unordered_dense::map<int, int>();
    m.insert({{1, 2}, {3, 4}, {5, 6}});
    REQUIRE(m.size() == 3U);
    REQUIRE(m[1] == 2);
    REQUIRE(m[3] == 4);
    REQUIRE(m[5] == 6);
}

TEST_CASE("insert_initializer_list_string") {
    auto m = ankerl::unordered_dense::map<int, std::string>();
    m.insert({{1, "a"}, {3, "b"}, {5, "c"}});
    REQUIRE(m.size() == 3U);
    REQUIRE(m[1] == "a");
    REQUIRE(m[3] == "b");
    REQUIRE(m[5] == "c");
}
