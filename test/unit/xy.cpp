#include <ankerl/unordered_dense.h>

#include <app/nanobench.h>

#include <doctest.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include <chrono>
#include <iomanip>
#include <iostream>

TEST_CASE("fill" * doctest::skip()) {
    auto data = ankerl::unordered_dense::set<uint32_t>();

    auto begin = std::chrono::steady_clock::now();

    auto rng = ankerl::nanobench::Rng(123);
    auto old_bucket_count = size_t();
    while (data.size() < 50'000'000) {
        data.insert(static_cast<uint32_t>(rng()));

        if (data.bucket_count() != old_bucket_count) {
            auto end = std::chrono::steady_clock::now();
            fmt::print("{:15.3f}s: {:>15} size, {:>15} bucket_count\n",
                       std::chrono::duration<double>(end - begin).count(),
                       data.size(),
                       data.bucket_count());
            old_bucket_count = data.bucket_count();
        }
    }
    auto end = std::chrono::steady_clock::now();
    fmt::print("{:15.3f}s: {:>15} size, {:>15} bucket_count\n",
               std::chrono::duration<double>(end - begin).count(),
               data.size(),
               data.bucket_count());
}
