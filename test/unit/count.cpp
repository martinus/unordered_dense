#include <ankerl/unordered_dense.h>

#include <doctest.h>

TEST_CASE("count") {
    auto map = ankerl::unordered_dense::map<int, int>();
    REQUIRE(map.count(123) == 0);
    REQUIRE(map.count(0) == 0);
    map[123];
    REQUIRE(map.count(123) == 1);
    REQUIRE(map.count(0) == 0);
}
