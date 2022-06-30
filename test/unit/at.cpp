#include <ankerl/unordered_dense.h>

#include <doctest.h>

using Map = ankerl::unordered_dense::map<int, int>;

TEST_CASE("at") {
    Map map;
    Map const& cmap = map;
    REQUIRE_THROWS_AS(map.at(123), std::out_of_range);
    REQUIRE_THROWS_AS(static_cast<void>(map.at(0)), std::out_of_range);
    REQUIRE_THROWS_AS(static_cast<void>(cmap.at(123)), std::out_of_range);
    REQUIRE_THROWS_AS(static_cast<void>(cmap.at(0)), std::out_of_range);
    map[123] = 333;
    REQUIRE(map.at(123) == 333);
    REQUIRE(cmap.at(123) == 333);
    REQUIRE_THROWS_AS(static_cast<void>(map.at(0)), std::out_of_range);
    REQUIRE_THROWS_AS(static_cast<void>(cmap.at(0)), std::out_of_range);
}
