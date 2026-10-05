// One id_map layout per binary for perf stat and objdump (#379): -DTAG=0 a 24 byte directory entry,
// -DTAG=1 the flag in the meta pointer. Builds n dense IDs, then ROUNDS x 1M independent lookups that
// use the miss, so the compiler keeps the bit test.
//   g++ -O3 -DNDEBUG -std=c++17 -DTAG=0 lookup_perf.cpp && taskset -c 2 perf stat -e cycles,instructions ./a.out 3500000 30
#include "id_map.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
struct value {
    std::uint32_t a, b;
};
using M = idm::id_map<std::uint32_t, value, 12, false, idm::tune<false, 1, 8, TAG != 0>>;
struct rng {
    std::uint64_t s;
    std::uint64_t operator()() {
        s += 0x9E3779B97F4A7C15ULL;
        auto z = s;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }
};
__attribute__((noinline)) std::uint64_t lookups(M const& m, std::uint64_t n, rng& r) {
    std::uint64_t sum = 0;
    for (int q = 0; q < 1000000; ++q) {
        auto const i = ((r() >> 32U) * n) >> 32U;
        auto const* v = m.find_ptr(static_cast<std::uint32_t>(i));
        sum += v != nullptr ? v->a : 7;
    }
    return sum;
}
int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s n rounds\n", argv[0]);
        return 1;
    }
    std::uint64_t n = std::strtoull(argv[1], nullptr, 10);
    int rounds = std::atoi(argv[2]);
    M m;
    for (std::uint64_t i = 0; i < n; ++i)
        m.emplace(static_cast<std::uint32_t>(i), value{static_cast<std::uint32_t>(i), 1});
    rng r{1};
    std::uint64_t s = lookups(m, n, r); // warm-up
    auto t0 = std::chrono::steady_clock::now();
    for (int k = 0; k < rounds; ++k)
        s += lookups(m, n, r);
    auto t1 = std::chrono::steady_clock::now();
    std::printf("n %llu  %.2f ns/lookup  (%llu)\n",
                (unsigned long long)n,
                std::chrono::duration<double, std::nano>(t1 - t0).count() / (rounds * 1e6),
                (unsigned long long)s);
}
