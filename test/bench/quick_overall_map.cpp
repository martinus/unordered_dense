#include <ankerl/unordered_dense.h> // for map, hash

#include <app/geomean.h>            // for geomean
#include <third-party/nanobench.h>  // for Rng, doNotOptimizeAway, Bench
#include <third-party/robin_hood.h> // for unordered_node_map, unordered_fl...

#include <doctest.h>  // for TestCase, skip, ResultBuilder
#include <fmt/core.h> // for print, format

#include <chrono>        // for duration, operator-, high_resolu...
#include <cstdint>       // for uint64_t
#include <cstring>       // for size_t, memcpy
#include <string>        // for string, basic_string, operator==
#include <string_view>   // for string_view, literals
#include <unordered_map> // for unordered_map, operator!=
#include <vector>        // for vector

using namespace std::literals;

namespace {

template <typename K>
inline auto init_key() -> K {
    return {};
}

template <typename T>
inline void randomize_key(ankerl::nanobench::Rng* rng, int n, T* key) {
    // we limit ourselfes to 32bit n
    auto limited = (((*rng)() >> 32U) * static_cast<uint64_t>(n)) >> 32U;
    *key = static_cast<T>(limited);
}

template <>
[[nodiscard]] inline auto init_key<std::string>() -> std::string {
    std::string str;
    str.resize(200);
    return str;
}

inline void randomize_key(ankerl::nanobench::Rng* rng, int n, std::string* key) {
    uint64_t k{};
    randomize_key(rng, n, &k);
    std::memcpy(key->data(), &k, sizeof(k));
}

// Random insert & erase
template <typename Map>
void bench_random_insert_erase(ankerl::nanobench::Bench* bench, std::string_view name) {
    bench->run(fmt::format("{} random insert erase", name), [&] {
        ankerl::nanobench::Rng rng(123);
        size_t verifier{};
        Map map;
        auto key = init_key<typename Map::key_type>();
        for (int n = 1; n < 20000; ++n) {
            for (int i = 0; i < 200; ++i) {
                randomize_key(&rng, n, &key);
                map[key];
                randomize_key(&rng, n, &key);
                verifier += map.erase(key);
            }
        }
        CHECK(verifier == 1994641U);
        CHECK(map.size() == 9987U);
    });
}

// iterate
template <typename Map>
void bench_iterate(ankerl::nanobench::Bench* bench, std::string_view name) {
    size_t num_elements = 5000;

    auto key = init_key<typename Map::key_type>();

    // insert
    bench->run(fmt::format("{} iterate while adding then removing", name), [&] {
        ankerl::nanobench::Rng rng(555);
        Map map;
        size_t result = 0;
        for (size_t n = 0; n < num_elements; ++n) {
            randomize_key(&rng, 1000000, &key);
            map[key] = n;
            for (auto const& key_val : map) {
                result += key_val.second;
            }
        }

        rng = ankerl::nanobench::Rng(555);
        do {
            randomize_key(&rng, 1000000, &key);
            map.erase(key);
            for (auto const& key_val : map) {
                result += key_val.second;
            }
        } while (!map.empty());

        CHECK(result == 62282755409U);
    });
}

// 111.903 222
// 112.023 123123
template <typename Map>
void bench_random_find(ankerl::nanobench::Bench* bench, std::string_view name) {

    bench->run(fmt::format("{} 50% probability to find", name), [&] {
        uint64_t const seed = 123123;
        ankerl::nanobench::Rng numbers_insert_rng(seed);
        size_t numbers_insert_rng_calls = 0;

        ankerl::nanobench::Rng numbers_search_rng(seed);
        size_t numbers_search_rng_calls = 0;

        ankerl::nanobench::Rng insertion_rng(123);

        size_t checksum = 0;
        size_t found = 0;
        size_t not_found = 0;

        Map map;
        auto key = init_key<typename Map::key_type>();
        for (size_t i = 0; i < 100000; ++i) {
            randomize_key(&numbers_insert_rng, 1000000, &key);
            ++numbers_insert_rng_calls;

            if (insertion_rng() & 1U) {
                map[key] = i;
            }

            // search 100 entries in the map
            for (size_t search = 0; search < 100; ++search) {
                randomize_key(&numbers_search_rng, 1000000, &key);
                ++numbers_search_rng_calls;

                auto it = map.find(key);
                if (it != map.end()) {
                    checksum += it->second;
                    ++found;
                } else {
                    ++not_found;
                }
                if (numbers_insert_rng_calls == numbers_search_rng_calls) {
                    numbers_search_rng = ankerl::nanobench::Rng(seed);
                    numbers_search_rng_calls = 0;
                }
            }
        }
        ankerl::nanobench::doNotOptimizeAway(checksum);
        ankerl::nanobench::doNotOptimizeAway(found);
        ankerl::nanobench::doNotOptimizeAway(not_found);
    });
}

template <typename Map>
void bench_all(ankerl::nanobench::Bench* bench, std::string_view name) {
    bench->title("benchmarking");
    bench->minEpochTime(100ms);
    bench_iterate<Map>(bench, name);
    bench_random_insert_erase<Map>(bench, name);
    bench_random_find<Map>(bench, name);
}

[[nodiscard]] auto geomean1(ankerl::nanobench::Bench const& bench) -> double {
    return geomean(bench.results(), [](ankerl::nanobench::Result const& result) {
        return result.median(ankerl::nanobench::Result::Measure::elapsed);
    });
}

} // namespace

// A relatively quick benchmark that should get a relatively good single number of how good the map
// is. It calculates geometric mean of several benchmarks.
TEST_CASE("bench_quick_overall_rhf" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    bench_all<robin_hood::unordered_flat_map<uint64_t, size_t>>(&bench, "robin_hood::unordered_flat_map<uint64_t, size_t>");
    bench_all<robin_hood::unordered_flat_map<std::string, size_t>>(&bench,
                                                                   "robin_hood::unordered_flat_map<std::string, size_t>");
    fmt::print("{} bench_quick_overall_rhf\n", geomean1(bench));

#ifdef ROBIN_HOOD_COUNT_ENABLED
    std::cout << robin_hood::counts() << std::endl;
#endif
}

TEST_CASE("bench_quick_overall_rhn" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    bench_all<robin_hood::unordered_node_map<uint64_t, size_t>>(&bench, "robin_hood::unordered_node_map<uint64_t, size_t>");
    bench_all<robin_hood::unordered_node_map<std::string, size_t>>(&bench,
                                                                   "robin_hood::unordered_node_map<std::string, size_t>");
    fmt::print("{} bench_quick_overall_rhn\n", geomean1(bench));

#ifdef ROBIN_HOOD_COUNT_ENABLED
    std::cout << robin_hood::counts() << std::endl;
#endif
}

TEST_CASE("bench_quick_overall_std" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    bench_all<std::unordered_map<uint64_t, size_t>>(&bench, "std::unordered_map<uint64_t, size_t>");
    bench_all<std::unordered_map<std::string, size_t>>(&bench, "std::unordered_map<std::string, size_t>");
    fmt::print("{} bench_quick_overall_map_std\n", geomean1(bench));
}

TEST_CASE("bench_quick_overall_udm" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    // bench.minEpochTime(1s);
    bench_all<ankerl::unordered_dense::map<uint64_t, size_t>>(&bench, "ankerl::unordered_dense::map<uint64_t, size_t>");
    bench_all<ankerl::unordered_dense::map<std::string, size_t, ankerl::unordered_dense::hash<std::string>>>(
        &bench, "ankerl::unordered_dense::map<std::string, size_t>");
    fmt::print("{} bench_quick_overall_map_udm\n", geomean1(bench));
}

TEST_CASE("bench_quick_overall_udm_bigbucket" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    // bench.minEpochTime(1s);
    bench_all<ankerl::unordered_dense::map<uint64_t,
                                           size_t,
                                           ankerl::unordered_dense::hash<uint64_t>,
                                           std::equal_to<uint64_t>,
                                           std::allocator<std::pair<uint64_t, size_t>>,
                                           ankerl::unordered_dense::bucket_type::big>>(
        &bench, "ankerl::unordered_dense::map<uint64_t, size_t>");

    bench_all<ankerl::unordered_dense::map<std::string,
                                           size_t,
                                           ankerl::unordered_dense::hash<std::string>,
                                           std::equal_to<std::string>,
                                           std::allocator<std::pair<std::string, size_t>>,
                                           ankerl::unordered_dense::bucket_type::big>>(
        &bench, "ankerl::unordered_dense::map<std::string, size_t>");
    fmt::print("{} bench_quick_overall_map_udm\n", geomean1(bench));
}

template <typename Map>
void test_big() {
    Map map;
    auto rng = ankerl::nanobench::Rng();
    for (uint64_t n = 0; n < 20000000; ++n) {
        map[rng()];
        map[rng()];
        map[rng()];
        map[rng()];
    }
    fmt::print("{} map.size()\n", map.size());
}

// 3346376 max RSS, 0:12.40
TEST_CASE("memory_map_huge_rhf" * doctest::test_suite("bench") * doctest::skip()) {
    test_big<robin_hood::unordered_flat_map<uint64_t, size_t>>();
}

// 2616352 max RSS, 0:24.72
TEST_CASE("memory_map_huge_rhn" * doctest::test_suite("bench") * doctest::skip()) {
    test_big<robin_hood::unordered_node_map<uint64_t, size_t>>();
}

// 3296524 max RSS, 0:50.76
TEST_CASE("memory_map_huge_uo" * doctest::test_suite("bench") * doctest::skip()) {
    test_big<std::unordered_map<uint64_t, size_t>>();
}

// 3149724 max RSS, 0:10.58
TEST_CASE("memory_map_huge_udm" * doctest::test_suite("bench") * doctest::skip()) {
    test_big<ankerl::unordered_dense::map<uint64_t, size_t>>();
}

template <typename Set>
void bench_consecutive_insert(char const* name) {
    auto before = std::chrono::high_resolution_clock::now();
    Set s{};
    for (uint64_t x = 0; x < 100000000; ++x) {
        s.insert(x);
    }
    auto after = std::chrono::high_resolution_clock::now();
    fmt::print("\t{}s, size={} for {}\n", std::chrono::duration<double>(after - before).count(), s.size(), name);
}

template <typename Set>
void bench_random_insert(char const* name) {
    ankerl::nanobench::Rng rng(23);
    auto before = std::chrono::high_resolution_clock::now();
    Set s{};
    for (uint64_t x = 0; x < 100000000; ++x) {
        s.insert(rng());
    }
    auto after = std::chrono::high_resolution_clock::now();
    fmt::print("\t{}s, size={} for {}\n", std::chrono::duration<double>(after - before).count(), s.size(), name);
}

template <typename Set>
void bench_shifted_insert(char const* name) {
    auto before = std::chrono::high_resolution_clock::now();
    Set s{};
    for (uint64_t x = 0; x < 100000000; ++x) {
        s.insert(x << 4U);
    }
    auto after = std::chrono::high_resolution_clock::now();
    fmt::print("\t{}s, size={} for {}\n", std::chrono::duration<double>(after - before).count(), s.size(), name);
}
