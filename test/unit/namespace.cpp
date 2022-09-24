#include <ankerl/unordered_dense.h>

#include <doctest.h>

static_assert(std::is_same_v<ankerl::unordered_dense::v1_3_3::map<int, int>, ankerl::unordered_dense::map<int, int>>);
static_assert(std::is_same_v<ankerl::unordered_dense::v1_3_3::hash<int>, ankerl::unordered_dense::hash<int>>);

TEST_CASE("version_namespace") {
    auto map = ankerl::unordered_dense::v1_3_3::map<int, int>{};
    REQUIRE(map.empty());
}
