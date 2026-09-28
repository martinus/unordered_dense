// Many small maps: build, look up at 50% hits, destroy, each timed, for sizes 0 to 32 (#304).
//
// One header, one key type per binary; the driver (small_maps.sh) builds one binary per header
// variant and compiler and alternates them. Each phase runs over `maps` maps of one size and is
// reported in ns per map; the index's bytes per map come from index_bytes(), which is what the
// index asked the allocator for.
//
//   clang++ -O3 -DNDEBUG -std=c++17 -I<include dir> [-DSTRING_KEY] scripts/ab/small_maps.cpp
//   ./a.out     # prints: key size build_ns lookup_ns destroy_ns index_bytes
#include <ankerl/unordered_dense.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct rng {
    std::uint64_t s = 0x243f6a8885a308d3ULL;
    auto operator()() -> std::uint64_t {
        s ^= s << 13U;
        s ^= s >> 7U;
        s ^= s << 17U;
        return s;
    }
};

#ifdef STRING_KEY
using bench_key = std::string;
constexpr char const* key_name = "string";
auto make_key(rng& r) -> bench_key {
    auto len = 8 + r() % 33;
    auto s = std::string(len, '\0');
    for (auto& c : s) {
        c = static_cast<char>('a' + r() % 26);
    }
    return s;
}
#else
using bench_key = std::uint64_t;
constexpr char const* key_name = "uint64";
auto make_key(rng& r) -> bench_key {
    return r() * UINT64_C(0x9E3779B97F4A7C15);
}
#endif

using map_t = ankerl::unordered_dense::map<bench_key, std::uint64_t>;
volatile std::uint64_t sink = 0;

auto now() {
    return std::chrono::steady_clock::now();
}
auto ns(std::chrono::steady_clock::duration d) -> double {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(d).count());
}

} // namespace

int main() {
    constexpr std::size_t maps = 20000;
    constexpr std::size_t sizes[] = {0, 1, 2, 4, 8, 12, 16, 32};
    auto r = rng{};
    // Every map gets its own keys, and a second set of the same size it does not hold, so a lookup
    // pass is 50% hits. Built before any timing.
    auto keys = std::vector<bench_key>(maps * 64);
    for (auto& k : keys) {
        k = make_key(r);
    }
    // Two passes over every size, the first untimed in effect: it faults in the heap every later
    // pass reuses, which otherwise lands on whichever size runs first.
    for (int pass = 0; pass < 2; ++pass) {
        for (auto n : sizes) {
            auto all = std::vector<map_t>(maps);
            auto t0 = now();
            for (std::size_t m = 0; m < maps; ++m) {
                auto const* k = &keys[m * 64];
                for (std::size_t i = 0; i < n; ++i) {
                    all[m].try_emplace(k[i], i);
                }
            }
            auto t1 = now();
            std::uint64_t acc = 0;
            for (std::size_t m = 0; m < maps; ++m) {
                auto const& map = all[m];
                auto const* k = &keys[m * 64];
                for (std::size_t i = 0; i < n; ++i) {
                    acc += map.count(k[i]);      // hit
                    acc += map.count(k[32 + i]); // miss: this map's keys stop at 32
                }
            }
            auto t2 = now();
            if (acc != maps * n) {
                std::printf("wrong: %zu hits, expected %zu\n", static_cast<std::size_t>(acc), maps * n);
                return 1;
            }
            std::size_t index_bytes = 0;
            for (auto const& map : all) {
                index_bytes += map.index_bytes();
            }
            all.clear();
            all.shrink_to_fit();
            auto t3 = now();
            sink = sink + acc;
            auto const per = static_cast<double>(maps);
            if (pass == 0) {
                continue;
            }
            std::printf("%s %zu %.1f %.1f %.1f %.0f\n",
                        key_name,
                        n,
                        ns(t1 - t0) / per,
                        ns(t2 - t1) / per,
                        ns(t3 - t2) / per,
                        static_cast<double>(index_bytes) / per);
        }
    }
    return 0;
}
