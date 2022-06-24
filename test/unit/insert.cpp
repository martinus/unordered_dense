#include <ankerl/unordered_dense_map.h>

#include <app/nanobench.h>
#include <app/robin_hood.h>

#include <doctest.h>
#include <fmt/format.h>

#include <chrono>
#include <unordered_map>

TEST_CASE("insert") {
    using Map = ankerl::unordered_dense_map<unsigned int, int>;
    auto map = Map();
    typename Map::value_type val(123U, 321);
    map.insert(val);
    REQUIRE(map.size() == 1);

    REQUIRE(map[123U] == 321);
}

template <typename Map>
void bench() {

    auto rng = ankerl::nanobench::Rng();
    auto before = std::chrono::steady_clock::now();
    for (size_t t = 0; t < 1000; ++t) {
        auto map = Map();
        for (size_t i = 0; i < 100000; ++i) {
            auto idx = rng.bounded(100000);
            map[idx] = idx;
        }
    }
    auto after = std::chrono::steady_clock::now();
    fmt::print("{}s\n", std::chrono::duration<double>(after - before).count());
}

TEST_CASE("bench_uo") {
    bench<std::unordered_map<uint64_t, uint64_t>>();
}

TEST_CASE("bench_rh") {
    bench<robin_hood::unordered_flat_map<uint64_t, uint64_t>>();
}

TEST_CASE("bench_udm") {
    bench<ankerl::unordered_dense_map<uint64_t, uint64_t>>();
}

TEST_CASE("random_insert") {
    auto dm = ankerl::unordered_dense_map<uint64_t, std::string>();
    auto uo = std::unordered_map<uint64_t, std::string>();

    auto rng = ankerl::nanobench::Rng();
    auto before = std::chrono::steady_clock::now();
    for (size_t t = 0; t < 1000; ++t) {
        for (size_t i = 0; i < 10000; ++i) {
            REQUIRE(dm.size() == uo.size());
            auto idx = rng.bounded(1000);
            REQUIRE(dm[idx] == uo[idx]);
            dm[idx] = std::to_string(i);
            uo[idx] = std::to_string(i);
            REQUIRE(dm.size() == uo.size());
        }
    }
    auto after = std::chrono::steady_clock::now();
    fmt::print("{}s\n", std::chrono::duration<double>(after - before).count());
}