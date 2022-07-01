#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <limits>

TEST_CASE("max_size") {
    auto map = ankerl::unordered_dense::map<int, int>();
    REQUIRE(map.max_size() == std::numeric_limits<uint32_t>::max());
}
