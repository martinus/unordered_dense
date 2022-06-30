#include <ankerl/unordered_dense.h>
#include <app/name_of_type.h>
#include <app/nanobench.h>
#include <app/robin_hood.h>

#include <chrono>
#include <doctest.h>
#include <fmt/format.h>

#include <array>
#include <unordered_map>

template <typename Map>
void bench() {
    static constexpr size_t numTotal = 4;

    auto requiredChecksum = std::array{200000, 25198620, 50197240, 75195862, 100194482};
    auto total = std::chrono::steady_clock::duration();

    for (size_t numFound = 0; numFound < 5; ++numFound) {
        auto title = fmt::format("random find {}% success {}", numFound * 100 / numTotal, name_of_type<Map>());
        auto rng = ankerl::nanobench::Rng(123);

        size_t checksum = 0;

        using Ary = std::array<bool, numTotal>;
        Ary insertRandom;
        insertRandom.fill(true);
        for (typename Ary::size_type i = 0; i < numFound; ++i) {
            insertRandom[i] = false;
        }

        auto anotherUnrelatedRng = ankerl::nanobench::Rng(987654321);
        auto const anotherUnrelatedRngInitialState = anotherUnrelatedRng.state();
        auto findRng = ankerl::nanobench::Rng(anotherUnrelatedRngInitialState);

        {
            static constexpr size_t numInserts = 200000;
            static constexpr size_t numFindsPerInsert = 500;
            static constexpr size_t numFindsPerIter = numFindsPerInsert * numTotal;

            Map map;
            size_t i = 0;
            size_t findCount = 0;
            auto before = std::chrono::steady_clock::now();
            do {
                // insert numTotal entries: some random, some sequential.
                rng.shuffle(insertRandom);
                for (bool isRandomToInsert : insertRandom) {
                    auto val = anotherUnrelatedRng();
                    if (isRandomToInsert) {
                        map[rng()] = static_cast<size_t>(1);
                    } else {
                        map[val] = static_cast<size_t>(1);
                    }
                    ++i;
                }

                // the actual benchmark code which should be as fast as possible
                for (size_t j = 0; j < numFindsPerIter; ++j) {
                    if (++findCount > i) {
                        findCount = 0;
                        findRng = ankerl::nanobench::Rng(anotherUnrelatedRngInitialState);
                    }
                    auto it = map.find(findRng());
                    if (it != map.end()) {
                        checksum += it->second;
                    }
                }
            } while (i < numInserts);
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
