#include <ankerl/unordered_dense.h> // for map, hash

#include <app/doctest.h>           // for TestCase, skip, ResultBuilder
#include <app/geomean.h>           // for geomean
#include <bench/workloads.h>       // for insert_erase, iterate, find_50, find_all, hash_strings
#include <third-party/nanobench.h> // for Bench

#include <fmt/format.h> // for print, format

#include <chrono>        // for duration, operator-, high_resolu...
#include <cstdint>       // for uint64_t
#include <deque>         // for deque
#include <string>        // for string, basic_string, operator==
#include <string_view>   // for string_view, literals
#include <unordered_map> // for unordered_map, operator!=
#include <vector>        // for vector

using namespace std::literals;

namespace {

template <typename Map>
void bench_random_insert_erase(ankerl::nanobench::Bench* bench, std::string_view name) {
    bench->run(fmt::format("{} random insert erase", name), [&] {
        auto r = workloads::insert_erase<Map>();
        CHECK(r.erased == 1994641U);
        CHECK(r.size == 9987U);
    });
}

template <typename Map>
void bench_iterate(ankerl::nanobench::Bench* bench, std::string_view name) {
    bench->run(fmt::format("{} iterate while adding then removing", name), [&] {
        CHECK(workloads::iterate<Map>() == 62282755409U);
    });
}

template <typename Map>
void bench_random_find(ankerl::nanobench::Bench* bench, std::string_view name) {
    bench->run(fmt::format("{} 50% probability to find", name), [&] {
        CHECK(workloads::find_50<Map>() == 124865472559U);
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

#if 0

// A relatively quick benchmark that should get a relatively good single number of how good the map
// is. It calculates geometric mean of several benchmarks.
TEST_CASE("bench_quick_overall_rhf" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    bench_all<robin_hood::unordered_flat_map<uint64_t, size_t>>(&bench, "robin_hood::unordered_flat_map<uint64_t, size_t>");
    bench_all<robin_hood::unordered_flat_map<std::string, size_t>>(&bench,
                                                                   "robin_hood::unordered_flat_map<std::string, size_t>");
    fmt::print("{} bench_quick_overall_rhf\n", geomean1(bench));

#    ifdef ROBIN_HOOD_COUNT_ENABLED
    std::cout << robin_hood::counts() << std::endl;
#    endif
}

TEST_CASE("bench_quick_overall_rhn" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    bench_all<robin_hood::unordered_node_map<uint64_t, size_t>>(&bench, "robin_hood::unordered_node_map<uint64_t, size_t>");
    bench_all<robin_hood::unordered_node_map<std::string, size_t>>(&bench,
                                                                   "robin_hood::unordered_node_map<std::string, size_t>");
    fmt::print("{} bench_quick_overall_rhn\n", geomean1(bench));

#    ifdef ROBIN_HOOD_COUNT_ENABLED
    std::cout << robin_hood::counts() << std::endl;
#    endif
}

#endif

using hash_t = ankerl::unordered_dense::hash<uint64_t>;
using eq_t = std::equal_to<uint64_t>;
using pair_t = std::pair<uint64_t, size_t>;

using hash_str_t = ankerl::unordered_dense::hash<std::string>;
using eq_str_t = std::equal_to<std::string>;
using pair_str_t = std::pair<std::string, size_t>;

TEST_CASE("bench_quick_overall_std" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    bench_all<std::unordered_map<uint64_t, size_t>>(&bench, "std::unordered_map<uint64_t, size_t>");
    bench_all<std::unordered_map<std::string, size_t>>(&bench, "std::unordered_map<std::string, size_t>");
    fmt::print("{} bench_quick_overall_map_std\n", geomean1(bench));
}

TEST_CASE("bench_quick_overall_udm" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    // bench.minEpochTime(1s);

    using map_t = ankerl::unordered_dense::map<uint64_t, size_t>;
    bench_all<map_t>(&bench, "ankerl::unordered_dense::map<uint64_t, size_t>");

    using map_str_t = ankerl::unordered_dense::map<std::string, size_t, hash_str_t>;
    bench_all<map_str_t>(&bench, "ankerl::unordered_dense::map<std::string, size_t>");

    fmt::print("{} bench_quick_overall_map_udm\n", geomean1(bench));
}

// The two ends of what a workload's hit rate can do to the probe; not part of the score.
TEST_CASE("bench_find_all_hits_or_misses_udm" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    bench.title("find").minEpochTime(100ms);
    using map_t = ankerl::unordered_dense::map<uint64_t, size_t>;
    using map_str_t = ankerl::unordered_dense::map<std::string, size_t, hash_str_t>;
    bench.run("map<uint64_t, size_t> all hits", [] {
        ankerl::nanobench::doNotOptimizeAway(workloads::find_all<map_t, true>());
    });
    bench.run("map<uint64_t, size_t> no hits", [] {
        ankerl::nanobench::doNotOptimizeAway(workloads::find_all<map_t, false>());
    });
    bench.run("map<std::string, size_t> all hits", [] {
        ankerl::nanobench::doNotOptimizeAway(workloads::find_all<map_str_t, true>());
    });
    bench.run("map<std::string, size_t> no hits", [] {
        ankerl::nanobench::doNotOptimizeAway(workloads::find_all<map_str_t, false>());
    });
}

// The string hash on its own; not part of the score. A lookup is ~224 instructions and only ~60
// of them are the hash, so a change to the hash is easier to resolve here than in the score.
TEST_CASE("bench_hash_string_udm" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    bench.title("hash").minEpochTime(100ms).unit("hash").batch(workloads::hash_keys().size());
    using map_str_t = ankerl::unordered_dense::map<std::string, size_t, hash_str_t>;
    bench.run("map<std::string, size_t> hash only", [] {
        ankerl::nanobench::doNotOptimizeAway(workloads::hash_strings<map_str_t>());
    });
}

TEST_CASE("bench_quick_overall_segmented_vector" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    // bench.minEpochTime(1s);
    using vec_t = ankerl::unordered_dense::segmented_vector<pair_t>;
    using map_t = ankerl::unordered_dense::segmented_map<uint64_t, size_t, hash_t, eq_t, vec_t>;
    bench_all<map_t>(&bench, "ankerl::unordered_dense::map<uint64_t, size_t> segmented_vector");

    using vec_str_t = ankerl::unordered_dense::segmented_vector<pair_str_t>;
    using map_str_t = ankerl::unordered_dense::map<std::string, size_t, hash_str_t, eq_str_t, vec_str_t>;
    bench_all<map_str_t>(&bench, "ankerl::unordered_dense::map<std::string, size_t> segmented_vector");

    fmt::print("{} bench_quick_overall_segmented_vector\n", geomean1(bench));
}

TEST_CASE("bench_quick_overall_deque" * doctest::test_suite("bench") * doctest::skip()) {
    ankerl::nanobench::Bench bench;
    // bench.minEpochTime(1s);

    using vec_t = std::deque<pair_t>;
    using map_t = ankerl::unordered_dense::map<uint64_t, size_t, hash_t, eq_t, vec_t>;
    bench_all<map_t>(&bench, "ankerl::unordered_dense::map<uint64_t, size_t> deque");

    using vec_str_t = std::deque<pair_str_t>;
    using map_str_t = ankerl::unordered_dense::map<std::string, size_t, hash_str_t, eq_str_t, vec_str_t>;
    bench_all<map_str_t>(&bench, "ankerl::unordered_dense::map<std::string, size_t> deque");

    fmt::print("{} bench_quick_overall_deque\n", geomean1(bench));
}

TEST_CASE("bench_quick_overall_udm_bigbucket" * doctest::test_suite("bench") * doctest::skip()) {
    using bucket_t = ankerl::unordered_dense::bucket_type::big;

    ankerl::nanobench::Bench bench;
    // bench.minEpochTime(1s);

    using alloc_t = std::allocator<pair_t>;
    using map_t = ankerl::unordered_dense::map<uint64_t, size_t, hash_t, eq_t, alloc_t, bucket_t>;
    bench_all<map_t>(&bench, "ankerl::unordered_dense::map<uint64_t, size_t>");

    using alloc_str_t = std::allocator<pair_str_t>;
    using map_str_t = ankerl::unordered_dense::map<std::string, size_t, hash_str_t, eq_str_t, alloc_str_t, bucket_t>;
    bench_all<map_str_t>(&bench, "ankerl::unordered_dense::map<std::string, size_t>");

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

#if 0

// 3346376 max RSS, 0:12.40
TEST_CASE("memory_map_huge_rhf" * doctest::test_suite("bench") * doctest::skip()) {
    test_big<robin_hood::unordered_flat_map<uint64_t, size_t>>();
}

// 2616352 max RSS, 0:24.72
TEST_CASE("memory_map_huge_rhn" * doctest::test_suite("bench") * doctest::skip()) {
    test_big<robin_hood::unordered_node_map<uint64_t, size_t>>();
}

#endif

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
