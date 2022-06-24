#include <ankerl/unordered_dense_map.h>

#include <app/nanobench.h>

#include <doctest.h>
#include <unordered_map>

TEST_CASE("insert") {
    using Map = ankerl::unordered_dense_map<unsigned int, int>;
    auto map = Map();
    typename Map::value_type val(123U, 321);
    map.insert(val);
    REQUIRE(map.size() == 1);

    REQUIRE(map[123U] == 321);
}

TEST_CASE("random_insert") {
    auto dm = ankerl::unordered_dense_map<uint64_t, std::string>();
    auto uo = std::unordered_map<uint64_t, std::string>();

    auto rng = ankerl::nanobench::Rng();
    for (size_t i = 0; i < 10000; ++i) {
        REQUIRE(dm.size() == uo.size());
        auto idx = rng.bounded(1000);
        REQUIRE(dm[idx] == uo[idx]);
        dm[idx] = std::to_string(i);
        uo[idx] = std::to_string(i);
        REQUIRE(dm.size() == uo.size());
    }
}