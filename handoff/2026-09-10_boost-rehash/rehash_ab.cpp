// Paths are relative to the repo root; see notes/index-design.md,
// "The gcc/clang gap in boost's rehash loop is not 1.5x".
//
//   for cxx in clang++ g++-15; do
//     $cxx -O2 -DNDEBUG -std=c++17 -w -I<boost> -Iinclude -Itest \
//         handoff/2026-09-10_boost-rehash/rehash_ab.cpp -o /tmp/rh_$cxx
//   done
//   # interleave the two, many rounds, and compare the MINIMA

// Isolated rehash, grow and shrink reported separately.
#include <boost/unordered/unordered_flat_map.hpp>
#include <ankerl/unordered_dense.h>
#include <bench/workloads.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using clk = std::chrono::steady_clock;
template <typename K> struct ah : ankerl::unordered_dense::hash<K> { using is_avalanching = void; };
template <typename K> auto keys(std::size_t n) {
    std::vector<K> v; v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) v.push_back(workloads::key_source<K>::get(i * 7919 + 13));
    return v;
}
template <typename K> void run(char const* name, std::size_t n, int reps) {
    workloads::tame_allocator();
    auto ks = keys<K>(n);
    boost::unordered_flat_map<K, std::size_t, ah<K>> m;
    for (std::size_t i = 0; i < n; ++i) m[ks[i]] = i;
    auto bc = m.bucket_count();
    std::vector<double> grow, shrink;
    for (int r = 0; r < reps; ++r) {
        auto t0 = clk::now(); m.rehash(bc * 2); auto t1 = clk::now();
        grow.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / double(n));
        auto t2 = clk::now(); m.rehash(bc); auto t3 = clk::now();
        shrink.push_back(std::chrono::duration<double, std::nano>(t3 - t2).count() / double(n));
    }
    std::size_t bad = 0;
    for (std::size_t i = 0; i < n; ++i) { auto it = m.find(ks[i]); if (it == m.end() || it->second != i) ++bad; }
    std::sort(grow.begin(), grow.end()); std::sort(shrink.begin(), shrink.end());
    std::printf("%-4s n=%-8zu grow min %6.2f med %6.2f   shrink min %6.2f med %6.2f  bad=%zu\n",
                name, n, grow.front(), grow[grow.size()/2], shrink.front(), shrink[shrink.size()/2], bad);
}
int main(int argc, char** argv) {
    std::size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 200000;
    int reps = argc > 2 ? std::atoi(argv[2]) : 15;
    char k = argc > 3 ? argv[3][0] : 'u';
    if (k == 'u') run<std::uint64_t>("u64", n, reps); else run<std::string>("str", n, reps);
}
