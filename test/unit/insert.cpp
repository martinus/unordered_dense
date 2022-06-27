#if 0
#    include <ankerl/unordered_dense_map.h>

#    include <app/name_of_type.h>
#    include <app/nanobench.h>
#    include <app/robin_hood.h>

#    include <doctest.h>
#    include <fmt/format.h>

#    include <chrono>
#    include <unordered_map>

TEST_CASE("insert") {
    using Map = ankerl::unordered_dense_map<unsigned int, int>;
    auto map = Map();
    typename Map::value_type val(123U, 321);
    map.insert(val);
    REQUIRE(map.size() == 1);

    REQUIRE(map[123U] == 321);
}

TEST_CASE("erase") {
    using Map = ankerl::unordered_dense_map<unsigned int, int>;
    auto map = Map();
    REQUIRE(map.size() == 0);
    map.try_emplace(11, 1234);
    REQUIRE(map.size() == 1);
    REQUIRE(map[11] == 1234);

    REQUIRE(0 == map.erase(123));
    REQUIRE(map.size() == 1);
    REQUIRE(0 == map.erase(1234));
    REQUIRE(map.size() == 1);
    REQUIRE(1 == map.erase(11));
    REQUIRE(map.size() == 0);
}

template <typename Map>
void bench() {
    auto const repeats = size_t{1000};
    auto const iterations = uint32_t{300000};
    auto const bounded = iterations / 3;

    auto rng = ankerl::nanobench::Rng(123);
    auto check = size_t{};
    auto before = std::chrono::steady_clock::now();
    for (size_t r = 0; r < repeats; ++r) {
        auto map = Map();
        for (size_t i = 0; i < iterations; ++i) {
            auto idx = rng.bounded(bounded);
            map[idx] = idx;

            idx = rng.bounded(bounded);
            check += map.erase(idx);
        }
        check += map.size();
    }
    auto after = std::chrono::steady_clock::now();
    fmt::print("{}s {}\n", std::chrono::duration<double>(after - before).count(), check);
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
}

TEST_CASE("random_insert_erase") {
    auto dm = ankerl::unordered_dense_map<uint64_t, std::string>();
    auto uo = std::unordered_map<uint64_t, std::string>();

    auto iterations = size_t{1000000};
    auto bounded = size_t{100000};

    auto rng = ankerl::nanobench::Rng();
    for (size_t i = 0; i < iterations; ++i) {
        // insert
        REQUIRE(dm.size() == uo.size());
        auto key = rng.bounded(bounded);
        REQUIRE(dm[key] == uo[key]);
        dm[key] = std::to_string(i);
        uo[key] = std::to_string(i);

        // erase
        REQUIRE(dm.size() == uo.size());
        key = rng.bounded(bounded);
        REQUIRE(dm.erase(key) == uo.erase(key));
        REQUIRE(dm.size() == uo.size());
    }
}

TEST_CASE("sizeof") {
    using Map = ankerl::unordered_dense_map<uint64_t, uint64_t>;
    REQUIRE(sizeof(std::vector<std::pair<uint64_t, uint64_t>>) == 24);
    REQUIRE(sizeof(Map) == 48);
}

#endif