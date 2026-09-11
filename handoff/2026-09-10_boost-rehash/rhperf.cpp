// nothing but rehashes, so perf stat counts the growth loop and little else
#include <boost/unordered/unordered_flat_map.hpp>
#include <ankerl/unordered_dense.h>
#include <bench/workloads.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
template <typename K> struct ah : ankerl::unordered_dense::hash<K> { using is_avalanching = void; };
int main(int argc, char** argv) {
    workloads::tame_allocator();
    int which = argc > 1 ? std::atoi(argv[1]) : 0;
    std::size_t n = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1000000;
    int reps = argc > 3 ? std::atoi(argv[3]) : 20;
    std::vector<std::uint64_t> ks(n);
    for (std::size_t i = 0; i < n; ++i) ks[i] = workloads::key_source<std::uint64_t>::get(i * 7919 + 13);
    std::size_t acc = 0;
    if (which == 0) {
        ankerl::unordered_dense::map<std::uint64_t, std::size_t> m;
        for (auto k : ks) m[k] = 1;
        auto bc = m.bucket_count();
        for (int r = 0; r < reps; ++r) { m.rehash(r % 2 ? bc : bc * 2); acc += m.bucket_count(); }
    } else {
        boost::unordered_flat_map<std::uint64_t, std::size_t, ah<std::uint64_t>> m;
        for (auto k : ks) m[k] = 1;
        auto bc = m.bucket_count();
        for (int r = 0; r < reps; ++r) { m.rehash(r % 2 ? bc : bc * 2); acc += m.bucket_count(); }
    }
    std::printf("%zu\n", acc);
}
