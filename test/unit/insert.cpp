#include <ankerl/unordered_dense.h>

#include <doctest.h>

TEST_CASE("insert") {
    using Map = ankerl::unordered_dense::map<unsigned int, int>;
    auto map = Map();
    typename Map::value_type val(123U, 321);
    map.insert(val);
    REQUIRE(map.size() == 1);

    REQUIRE(map[123U] == 321);
}
