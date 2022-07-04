#include <ankerl/unordered_dense.h>
#include <app/nanobench.h>
#include <app/robin_hood.h>

#include <cstdint>
#include <doctest.h>
#include <fmt/format.h>

#include <iostream>
#include <unordered_map>

namespace {

template <typename Map>
void bench(std::string_view name) {
    Map a;
    Map b;
    ankerl::nanobench::Rng rng(123);

    ankerl::nanobench::Bench bench;
    for (size_t j = 0; j < 10000; ++j) {
        a[rng()];
        b[rng()];
    }
    bench.run(fmt::format("swap {}", name), [&] {
        std::swap(a, b);
    });
    ankerl::nanobench::doNotOptimizeAway(&a);
    ankerl::nanobench::doNotOptimizeAway(&b);
}

} // namespace

TEST_CASE("bench_swap_rhn" * doctest::test_suite("bench") * doctest::skip()) {
    bench<robin_hood::unordered_node_map<uint64_t, uint64_t>>("robin_hood::unordered_node_map");
}

TEST_CASE("bench_swap_rhf" * doctest::test_suite("bench") * doctest::skip()) {
    bench<robin_hood::unordered_flat_map<uint64_t, uint64_t>>("robin_hood::unordered_flat_map");
}

TEST_CASE("bench_swap_std" * doctest::test_suite("bench") * doctest::skip()) {
    bench<std::unordered_map<uint64_t, uint64_t>>("std::unordered_map");
}

TEST_CASE("bench_swap_udm" * doctest::test_suite("bench") * doctest::skip()) {
    bench<ankerl::unordered_dense::map<uint64_t, uint64_t>>("ankerl::unordered_dense::map");
}
