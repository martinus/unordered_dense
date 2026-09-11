#include <absl/container/flat_hash_map.h>
#include <boost/unordered/unordered_flat_map.hpp>
#include <ankerl/unordered_dense.h>
#include <bench/workloads.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using clk = std::chrono::steady_clock;
template <typename K> struct ah : ankerl::unordered_dense::hash<K> { using is_avalanching = void; };
template <typename K> auto keys(std::size_t n) { std::vector<K> v; v.reserve(n); for (std::size_t i = 0; i < n; ++i) v.push_back(workloads::key_source<K>::get(i * 7919 + 13)); return v; }
template <typename M, typename K> void one(char const* name, std::vector<K> const& ks, int reps) {
    double be = 1e30, br = 1e30;
    for (int r = 0; r < reps; ++r) { M m; auto t0 = clk::now(); for (auto const& k : ks) m[k]; auto t1 = clk::now();
        be = std::min(be, std::chrono::duration<double, std::nano>(t1-t0).count()/double(ks.size())); }
    for (int r = 0; r < reps; ++r) { M m; m.reserve(ks.size()); auto t0 = clk::now(); for (auto const& k : ks) m[k]; auto t1 = clk::now();
        br = std::min(br, std::chrono::duration<double, std::nano>(t1-t0).count()/double(ks.size())); }
    std::printf("  %-16s from empty %7.2f   reserved %7.2f   growth %7.2f ns/el (%4.1f%%)\n", name, be, br, be-br, 100.0*(be-br)/be);
}
template <typename V, typename K> void row(char const* what, std::vector<K> const& ks, int reps) {
    std::printf("%s\n", what);
    one<ankerl::unordered_dense::map<K, V, ah<K>>>("unordered_dense", ks, reps);
    one<boost::unordered_flat_map<K, V, ah<K>>>("boost", ks, reps);
    one<absl::flat_hash_map<K, V, ah<K>>>("abseil", ks, reps);
}
int main(int argc, char** argv) {
    workloads::tame_allocator();
    std::size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 200000;
    int reps = argc > 2 ? std::atoi(argv[2]) : 8;
    auto ku = keys<std::uint64_t>(n);
    row<std::size_t>("uint64_t key, 8 byte value", ku, reps);
    row<workloads::big_value>("uint64_t key, 64 byte value", ku, reps);
    row<std::size_t>("std::string key, 8 byte value", keys<std::string>(n), reps);
}
