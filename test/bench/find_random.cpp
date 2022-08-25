#include <ankerl/unordered_dense.h> // for map

#include <app/name_of_type.h>       // for name_of_type
#include <third-party/nanobench.h>  // for Rng
#include <third-party/robin_hood.h> // for unordered_flat_map

#include <doctest.h>  // for TestCase, skip, ResultBuilder
#include <fmt/core.h> // for format, print

#include <algorithm>     // for fill_n
#include <array>         // for array
#include <chrono>        // for duration, operator-, steady_clock
#include <cstddef>       // for size_t
#include <unordered_map> // for unordered_map, operator!=
#include <vector>        // for vector

template <typename Map>
void bench() {
    static constexpr size_t num_total = 4;

    auto requiredChecksum = std::array{200000, 25198620, 50197240, 75195862, 100194482};
    auto total = std::chrono::steady_clock::duration();

    for (size_t numFound = 0; numFound < 5; ++numFound) {
        auto title = fmt::format("random find {}% success {}", numFound * 100 / num_total, name_of_type<Map>());
        auto rng = ankerl::nanobench::Rng(123);

        size_t checksum = 0;

        using ary_t = std::array<bool, num_total>;
        auto insert_random = ary_t();
        insert_random.fill(true);
        for (typename ary_t::size_type i = 0; i < numFound; ++i) {
            insert_random[i] = false;
        }

        auto another_unrelated_rng = ankerl::nanobench::Rng(987654321);
        auto const another_unrelated_rng_initial_state = another_unrelated_rng.state();
        auto find_rng = ankerl::nanobench::Rng(another_unrelated_rng_initial_state);

        {
            static constexpr size_t num_inserts = 200000;
            static constexpr size_t num_finds_per_insert = 500;
            static constexpr size_t num_finds_per_iter = num_finds_per_insert * num_total;

            Map map;
            size_t i = 0;
            size_t find_count = 0;
            auto before = std::chrono::steady_clock::now();
            do {
                // insert numTotal entries: some random, some sequential.
                rng.shuffle(insert_random);
                for (bool is_random_to_insert : insert_random) {
                    auto val = another_unrelated_rng();
                    if (is_random_to_insert) {
                        map[static_cast<size_t>(rng())] = static_cast<size_t>(1);
                    } else {
                        map[static_cast<size_t>(val)] = static_cast<size_t>(1);
                    }
                    ++i;
                }

                // the actual benchmark code which should be as fast as possible
                for (size_t j = 0; j < num_finds_per_iter; ++j) {
                    if (++find_count > i) {
                        find_count = 0;
                        find_rng = ankerl::nanobench::Rng(another_unrelated_rng_initial_state);
                    }
                    auto it = map.find(static_cast<size_t>(find_rng()));
                    if (it != map.end()) {
                        checksum += it->second;
                    }
                }
            } while (i < num_inserts);
            checksum += map.size();
            auto after = std::chrono::steady_clock::now();
            total += after - before;
            fmt::print("{}s {}\n", std::chrono::duration<double>(after - before).count(), title);
        }
        REQUIRE(checksum == requiredChecksum[numFound]);
    }
    fmt::print("{}s total\n", std::chrono::duration<double>(total).count());
}

// 26.81
TEST_CASE("bench_find_random_uo" * doctest::test_suite("bench") * doctest::skip()) {
    bench<std::unordered_map<size_t, size_t>>();
}

// 10.55
TEST_CASE("bench_find_random_rh" * doctest::test_suite("bench") * doctest::skip()) {
    bench<robin_hood::unordered_flat_map<size_t, size_t>>();
}

// 8.87
TEST_CASE("bench_find_random_udm" * doctest::test_suite("bench") * doctest::skip()) {
    bench<ankerl::unordered_dense::map<size_t, size_t>>();
}
