#include <ankerl/unordered_dense.h>

#include <doctest.h>

TEST_CASE("reserve") {
    auto map = ankerl::unordered_dense::map<int, int>();
    REQUIRE(map.values().capacity() <= 1000U);
    map.reserve(1000);
    REQUIRE(map.values().capacity() >= 1000U);
    REQUIRE(0U == map.values().size());
}
