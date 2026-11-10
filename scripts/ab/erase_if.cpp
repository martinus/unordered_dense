// erase_if at a range of sizes and erased fractions: the header's std::erase_if against
// extract + std::remove_if + replace through the public API.
// Each cell: a fresh copy of a master map (copied and destroyed outside the clock), median of rounds.
#include <ankerl/unordered_dense.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct rng_t {
    uint64_t s;
    auto operator()() -> uint64_t {
        s += UINT64_C(0x9E3779B97F4A7C15);
        auto z = s;
        z = (z ^ (z >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27U)) * UINT64_C(0x94D049BB133111EB);
        return z ^ (z >> 31U);
    }
};

template <typename K>
auto make_key(uint64_t v) -> K {
    if constexpr (std::is_same_v<K, std::string>) {
        // 8..135 bytes, skewed short, like the scored workloads
        auto const spread = (v * UINT64_C(0x9E3779B97F4A7C15)) >> 56U;
        auto const len = 8 + ((spread * spread) >> 9U);
        auto s = std::string(len, 'x');
        std::memcpy(s.data(), &v, sizeof(v));
        return s;
    } else {
        return v;
    }
}

struct current {
    template <typename M>
    static auto run(M& m, uint64_t threshold) -> size_t {
        return std::erase_if(m, [threshold](auto const& kv) {
            return kv.second < threshold;
        });
    }
};

template <int Mode>
struct proto {
    template <typename M>
    static auto run(M& m, uint64_t threshold) -> size_t {
        return m.template erase_if_proto<Mode>([threshold](auto const& kv) {
            return kv.second < threshold;
        });
    }
};

// Enough copies that the timed region covers about two million elements; the copies are made
// and destroyed outside the clock.
template <typename Variant, typename M>
auto timed(M const& master, uint64_t threshold, size_t* erased) -> double {
    auto const reps = (std::max)(size_t{1}, size_t{2000000} / master.size());
    auto maps = std::vector<M>(reps, master);
    auto total = size_t{};
    auto const t0 = std::chrono::steady_clock::now();
    for (auto& m : maps) {
        total += Variant::run(m, threshold);
    }
    auto const t1 = std::chrono::steady_clock::now();
    *erased = total / reps;
    for (auto const& m : maps) {
        if (m.size() + *erased != master.size()) {
            std::abort();
        }
    }
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(reps);
}

auto median(std::vector<double> v) -> double {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

template <typename K>
void sweep(char const* name, int rounds) {
    using map_t = ankerl::unordered_dense::map<K, uint64_t>;
    for (size_t n : {size_t{1000}, size_t{10000}, size_t{100000}, size_t{1000000}, size_t{4000000}}) {
        auto master = map_t{};
        auto rng = rng_t{n};
        while (master.size() < n) {
            auto const v = rng();
            master.try_emplace(make_key<K>(v), rng() >> 32U); // value uniform in [0, 2^32)
        }
        for (double frac : {0.001, 0.01, 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.4, 0.5, 0.7, 0.95}) {
            auto const threshold = static_cast<uint64_t>(frac * 4294967296.0);
            auto a = std::vector<double>{};
            auto b = std::vector<double>{};
            auto c = std::vector<double>{};
            auto d = std::vector<double>{};
            auto ea = size_t{};
            auto eb = size_t{};
            for (int r = 0; r < rounds; ++r) {
                a.push_back(timed<current>(master, threshold, &ea));
                b.push_back(timed<proto<0>>(master, threshold, &eb));
                if (ea != eb) {
                    std::abort();
                }
                c.push_back(timed<proto<1>>(master, threshold, &eb));
                if (ea != eb) {
                    std::abort();
                }
                d.push_back(timed<proto<-1>>(master, threshold, &eb));
                if (ea != eb) {
                    std::abort();
                }
            }
            auto const dn = static_cast<double>(n);
            auto const ma = median(a);
            std::printf(
                "%-6s n=%8zu frac=%5.3f  ns per map element: current %7.2f  each %7.2f  rebuild %7.2f  gated %7.2f | current/gated %5.2f  best-of-two/gated %5.2f\n",
                name,
                n,
                frac,
                ma / dn,
                median(b) / dn,
                median(c) / dn,
                median(d) / dn,
                ma / median(d),
                (std::min)(median(b), median(c)) / median(d));
            std::fflush(stdout);
        }
    }
}

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc > 3) {
        // one variant alone, for perf: <variant 0=current 1=each 2=rebuild> <n> <frac>
        using map_t = ankerl::unordered_dense::map<uint64_t, uint64_t>;
        auto const n = static_cast<size_t>(std::atoll(argv[2]));
        auto const threshold = static_cast<uint64_t>(std::atof(argv[3]) * 4294967296.0);
        auto master = map_t{};
        auto rng = rng_t{n};
        while (master.size() < n) {
            auto const v = rng();
            master.try_emplace(v, rng() >> 32U);
        }
        auto e = size_t{};
        auto t = std::vector<double>{};
        for (int r = 0; r < 30; ++r) {
            switch (argv[1][0]) {
            case '0':
                t.push_back(timed<current>(master, threshold, &e));
                break;
            case '1':
                t.push_back(timed<proto<0>>(master, threshold, &e));
                break;
            default:
                t.push_back(timed<proto<1>>(master, threshold, &e));
                break;
            }
        }
        std::printf("%.2f ns per map element, %zu erased\n", median(t) / static_cast<double>(n), e);
        return 0;
    }
    auto const rounds = argc > 1 ? std::atoi(argv[1]) : 5;
    sweep<uint64_t>("u64", rounds);
    sweep<std::string>("string", rounds);
}
