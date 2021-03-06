#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <cstdint> // for uint64_t

TEST_CASE("contains") {
    auto map = ankerl::unordered_dense::map<uint64_t, uint64_t>();

    REQUIRE(!map.contains(0));
    REQUIRE(!map.contains(123));
    map[123];
    REQUIRE(!map.contains(0));
    REQUIRE(map.contains(123));
    map.clear();
    REQUIRE(!map.contains(0));
    REQUIRE(!map.contains(123));
}
