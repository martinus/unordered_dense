#include <ankerl/unordered_dense_map.h>

#include <doctest.h>

#include <vector>

TEST_CASE("iterators_insert") {
    auto v = std::vector<std::pair<int, int>>();
    v.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        v.emplace_back(i, i);
    }

    auto map = ankerl::unordered_dense_map<int, int>(v.begin(), v.end());
    REQUIRE(map.size() == v.size());
    for (auto const& kv : v) {
        REQUIRE(map.count(kv.first) == 1);
        auto it = map.find(kv.first);
        REQUIRE(it != map.end());
    }
}
