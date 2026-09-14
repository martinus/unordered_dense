// Is "ns per lookup" latency or throughput, and is a million-entry table in L3?
//
// Both questions came from a reader of the index-structures post, and both were fair. Every lookup
// harness here draws its next key from an rng, so several lookups are in flight and what comes out
// is reciprocal throughput. A program that looks up in a loop gets the same overlap, so it is worth
// measuring, but it is not the latency of one lookup and the two do not rank the maps the same way.
//
//   scripts/ab/latency.cpp, built as:
//     clang++ -O3 -DNDEBUG -std=c++20 -Iinclude scripts/ab/latency.cpp -o lat
//     taskset -c 2 ./lat <entries> <lookups> [udm|boost]
//
// Two modes over the same map, same keys, same count:
//   indep  the next key comes from an rng, so several lookups are in flight at once. This is what
//          scripts/ab/maps.h does, and it measures reciprocal *throughput*.
//   chain  the next key is the value the last lookup returned, so nothing can start early. Latency.
//
// The two loops have to do the same work per lookup apart from the dependency. The first version of
// the independent one read a shuffled index array to pick its key, which at a million entries is an
// 8 MB array and contributed its own cache misses: it measured those, and read *slower* than the
// chain. Keys are mix(1..n) so an in-register rng picks a present key with no memory access.
#include <ankerl/unordered_dense.h>
#include <boost/unordered/unordered_flat_map.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {
auto mix(std::uint64_t x) -> std::uint64_t { // a bijection, so keys are not sequential
    x ^= x >> 33U; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33U;
    x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33U; return x;
}
template <typename Map>
void run(char const* name, std::size_t n, std::size_t reps) {
    auto order = std::vector<std::uint64_t>(n);
    std::iota(order.begin(), order.end(), 1);
    std::shuffle(order.begin(), order.end(), std::mt19937_64(123));
    auto m = Map();
    m.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        m[mix(order[i])] = mix(order[(i + 1) % n]); // m[k] holds the next key: a chase
    }
    if (m.size() != n) { std::printf("bad fill\n"); return;
    }
    auto const t0 = std::chrono::steady_clock::now();
    auto k = mix(order[0]);
    std::uint64_t acc = 0;
    for (std::size_t i = 0; i < reps; ++i) { k = m.find(k)->second; acc += k; }
    auto const t1 = std::chrono::steady_clock::now();
    // The independent loop must do the same work per lookup as the chain, minus the dependency.
    // Keys are mix(1..n), so an in-register rng picks a present key with no memory access of its
    // own; reading a shuffled index array instead added its own cache misses and measured those.
    std::uint64_t r = 12345;
    std::uint64_t acc2 = 0;
    for (std::size_t i = 0; i < reps; ++i) {
        r ^= r << 13U; r ^= r >> 7U; r ^= r << 17U;
        acc2 += m.find(mix(1 + r % n))->second;
    }
    auto const t2 = std::chrono::steady_clock::now();
    auto ns = [](auto a, auto b, std::size_t r) {
        return std::chrono::duration<double, std::nano>(b - a).count() / static_cast<double>(r);
    };
    std::printf("%-8s n=%-9zu chain(latency) %7.2f ns   indep(throughput) %7.2f ns   ratio %.2fx  [%llu]\n",
                name, n, ns(t0, t1, reps), ns(t1, t2, reps), ns(t0, t1, reps) / ns(t1, t2, reps),
                static_cast<unsigned long long>(acc ^ acc2));
}
} // namespace

auto main(int argc, char** argv) -> int {
    auto const n = argc > 1 ? std::stoull(argv[1]) : 1000000;
    auto const reps = argc > 2 ? std::stoull(argv[2]) : 20000000;
    char const* only = argc > 3 ? argv[3] : "";
    if (std::strcmp(only, "udm") == 0) { run<ankerl::unordered_dense::map<std::uint64_t, std::uint64_t>>("udm", n, reps); return 0; }
    if (std::strcmp(only, "boost") == 0) { run<boost::unordered_flat_map<std::uint64_t, std::uint64_t>>("boost", n, reps); return 0; }
    run<ankerl::unordered_dense::map<std::uint64_t, std::uint64_t>>("udm", n, reps);
    run<boost::unordered_flat_map<std::uint64_t, std::uint64_t>>("boost", n, reps);
    return 0;
}
