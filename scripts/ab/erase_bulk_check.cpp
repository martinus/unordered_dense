#include <algorithm>
#include <ankerl/unordered_dense.h>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
template <typename K>
K mk(uint64_t v) {
    if constexpr (std::is_same_v<K, std::string>)
        return std::to_string(v) + std::string(v % 40, 'x');
    else
        return v;
}
template <typename C>
bool same(C const& x, C const& y) {
    return x.size() == y.size() && std::equal(x.begin(), x.end(), y.begin());
}
template <typename Map>
void go() {
    std::mt19937_64 rng(42);
    for (int iter = 0; iter < 3000; ++iter) {
        size_t n = rng() % 700;
        Map ref;
        while (ref.size() < n)
            ref.try_emplace(mk<typename Map::key_type>(rng() % 5000), rng() % 1000);
        // churn a bit so the index has drift
        for (int c = 0; c < 50 && !ref.empty(); ++c) {
            ref.erase(ref.begin() + static_cast<std::ptrdiff_t>(rng() % ref.size()));
            ref.try_emplace(mk<typename Map::key_type>(rng() % 5000), rng() % 1000);
        }
        uint64_t thr = rng() % 1100;
        auto pred = [thr](auto const& kv) {
            return kv.second < thr;
        };
        Map a = ref, b = ref, c = ref, d = ref;
        auto ea = std::erase_if(a, pred);
        auto eb = b.template erase_if_proto<0>(pred);
        auto ec = c.template erase_if_proto<1>(pred);
        auto ed = d.template erase_if_proto<-1>(pred);
        if (ea != eb || ea != ec || ea != ed || !same(a.values(), b.values()) || !same(a.values(), c.values()) ||
            !same(a.values(), d.values())) {
            std::puts("erase_if MISMATCH");
            std::abort();
        }
        for (Map* m : {&b, &c, &d}) {
            for (auto const& kv : a)
                if (m->find(kv.first) == m->end() || m->find(kv.first)->second != kv.second) {
                    std::puts("lookup MISMATCH");
                    std::abort();
                }
            for (auto const& kv : ref)
                if ((a.find(kv.first) == a.end()) != (m->find(kv.first) == m->end())) {
                    std::puts("miss MISMATCH");
                    std::abort();
                }
            m->try_emplace(mk<typename Map::key_type>(999999), 1);
            if (m->size() != a.size() + 1) {
                std::printf("size after insert: %zu vs %zu, n=%zu thr=%lu erased=%zu\n",
                            m->size(),
                            a.size(),
                            ref.size(),
                            thr,
                            static_cast<size_t>(ea));
                std::abort();
            }
        }
        // range erase
        Map r0 = ref, r1 = ref, r2 = ref;
        size_t f = ref.empty() ? 0 : rng() % (ref.size() + 1), l = f + (ref.size() - f ? rng() % (ref.size() - f + 1) : 0);
        auto i0 = r0.erase(r0.cbegin() + static_cast<std::ptrdiff_t>(f), r0.cbegin() + static_cast<std::ptrdiff_t>(l));
        auto i1 = r1.template erase_range_proto<1>(r1.cbegin() + static_cast<std::ptrdiff_t>(f),
                                                   r1.cbegin() + static_cast<std::ptrdiff_t>(l));
        auto i2 = r2.template erase_range_proto<-1>(r2.cbegin() + static_cast<std::ptrdiff_t>(f),
                                                    r2.cbegin() + static_cast<std::ptrdiff_t>(l));
        if (!same(r0.values(), r1.values()) || !same(r0.values(), r2.values()) || (i0 - r0.begin()) != (i1 - r1.begin()) ||
            (i0 - r0.begin()) != (i2 - r2.begin())) {
            std::puts("range MISMATCH");
            std::abort();
        }
        for (auto const& kv : ref)
            if ((r0.find(kv.first) == r0.end()) != (r1.find(kv.first) == r1.end())) {
                std::puts("range miss MISMATCH");
                std::abort();
            }
        for (auto const& kv : r0)
            if (r1.find(kv.first)->second != kv.second) {
                std::puts("range value");
                std::abort();
            }
    }
}
int main() {
    go<ankerl::unordered_dense::map<uint64_t, uint64_t>>();
    go<ankerl::unordered_dense::map<std::string, uint64_t>>();
    go<ankerl::unordered_dense::segmented_map<std::string, uint64_t>>();
    std::puts("ok");
}
