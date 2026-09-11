#include <boost/unordered/unordered_flat_map.hpp>
#include <ankerl/unordered_dense.h>
#include <bench/workloads.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
template <typename K> auto keys(std::size_t n) { std::vector<K> v; v.reserve(n); for (std::size_t i = 0; i < n; ++i) v.push_back(workloads::key_source<K>::get(i * 7919 + 13)); return v; }
template <typename K> void run(char const* name, std::size_t n, int reps) {
    workloads::tame_allocator();
    auto ks = keys<K>(n);
    using clk = std::chrono::steady_clock;
    boost::unordered_flat_map<K, std::size_t, ankerl::unordered_dense::hash<K>> m;
    for (std::size_t i = 0; i < n; ++i) m[ks[i]] = i;
    double best = 1e30; auto bc = m.bucket_count();
    for (int r = 0; r < reps; ++r) {
        auto target = (r % 2 == 0) ? bc * 2 : bc;
        auto t0 = clk::now(); m.rehash(target); auto t1 = clk::now();
        if (m.bucket_count() == ((r % 2 == 0) ? bc : bc * 2)) std::abort();
        best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(n));
    }
    std::size_t bad = 0;
    for (std::size_t i = 0; i < n; ++i) if (auto it = m.find(ks[i]); it == m.end() || it->second != i) ++bad;
    double bbest = 1e30;
    for (int r = 0; r < reps / 4 + 1; ++r) {
        boost::unordered_flat_map<K, std::size_t, ankerl::unordered_dense::hash<K>> b;
        auto t0 = clk::now();
        for (std::size_t i = 0; i < n; ++i) b[ks[i]] = i;
        auto t1 = clk::now();
        bbest = std::min(bbest, std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(n));
    }
    std::printf("%-4s n=%-8zu rehash %6.2f ns/el   build %6.2f ns/el   bad=%zu\n", name, n, best, bbest, bad);
}
int main(int argc, char** argv) {
    std::size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 200000;
    int reps = argc > 2 ? std::atoi(argv[2]) : 12;
    run<std::uint64_t>("u64", n, reps);
    run<std::string>("str", n, reps);
}
