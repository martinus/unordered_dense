// Build a segmented_map from empty, destroy it, and build it again, in one process: what the order
// the map tears itself down in costs the build that comes after it. One key kind per binary, picked
// with -D:
//
//   KIND_U64    segmented_map<uint64_t, uint64_t>
//   KIND_STR    segmented_map<std::string, uint64_t>, 8 to 135 bytes skewed short, as the workloads
//   KIND_OWNED  segmented_map<uint64_t, std::unique_ptr<std::array<std::uint8_t, 96>>>: one allocation
//               of the caller's per entry, interleaved with the map's own, which is Bonxai's shape
//               (a root map whose values own heap memory) reduced to the map
//
// Prints, for each size: the first build (a fresh heap), the median of the warm builds after it, the
// median teardown, and the median of what glibc gave back to the kernel at each teardown (the main
// arena before and after, from mallinfo2). Times in ms.
//
//   clang++ -O3 -DNDEBUG -std=c++17 -I<include dir> -DKIND_U64 scripts/ab/teardown_order.cpp -o t
//   ./t 7 50000 200000 1000000
//
// scripts/ab/teardown_order.sh builds main's header against the working tree's and alternates them.
#include <ankerl/unordered_dense.h>

#include <malloc.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
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

#if defined(KIND_U64)
using map_t = ankerl::unordered_dense::segmented_map<std::uint64_t, std::uint64_t>;
constexpr char const* kind = "u64";
auto key_at(rng& r) -> std::uint64_t {
    return r() * UINT64_C(0x9E3779B97F4A7C15);
}
void put(map_t& m, std::uint64_t const& k, std::size_t i) {
    m.try_emplace(k, i);
}
#elif defined(KIND_STR)
using map_t = ankerl::unordered_dense::segmented_map<std::string, std::uint64_t>;
constexpr char const* kind = "str";
auto key_at(rng& r) -> std::string {
    auto const x = r() % 128U;
    auto s = std::string(8 + x * x / 128U, '\0'); // 8 to 135 bytes, skewed short
    for (auto& c : s) {
        c = static_cast<char>('a' + r() % 26U);
    }
    return s;
}
void put(map_t& m, std::string const& k, std::size_t i) {
    m.try_emplace(k, i);
}
#elif defined(KIND_OWNED)
using payload = std::array<std::uint8_t, 96>;
using map_t = ankerl::unordered_dense::segmented_map<std::uint64_t, std::unique_ptr<payload>>;
constexpr char const* kind = "owned";
auto key_at(rng& r) -> std::uint64_t {
    return r() * UINT64_C(0x9E3779B97F4A7C15);
}
void put(map_t& m, std::uint64_t const& k, std::size_t i) {
    auto p = std::make_unique<payload>();
    (*p)[0] = static_cast<std::uint8_t>(i);
    m.try_emplace(k, std::move(p));
}
#else
#    error "define KIND_U64, KIND_STR or KIND_OWNED"
#endif

using clk = std::chrono::steady_clock;
auto ms(clk::duration d) -> double {
    return std::chrono::duration<double, std::milli>(d).count();
}
auto median(std::vector<double> v) -> double {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

volatile std::size_t sink = 0;

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s rounds size...\n", argv[0]);
        return 1;
    }
    auto const rounds = std::atoi(argv[1]);
    for (int a = 2; a < argc; ++a) {
        auto const n = static_cast<std::size_t>(std::atol(argv[a]));
        auto r = rng{};
        using key_type = std::decay_t<decltype(key_at(r))>;
        auto keys = std::vector<key_type>();
        keys.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            keys.push_back(key_at(r));
        }
        auto first_build = 0.0;
        auto builds = std::vector<double>();
        auto teardowns = std::vector<double>();
        auto released = std::vector<double>();
        for (int round = 0; round < rounds; ++round) {
            auto* m = new map_t();
            auto const t0 = clk::now();
            for (std::size_t i = 0; i < n; ++i) {
                put(*m, keys[i], i);
            }
            auto const t1 = clk::now();
            sink = sink + m->size();
            auto const arena_before = mallinfo2().arena;
            auto const t2 = clk::now();
            delete m;
            auto const t3 = clk::now();
            auto const arena_after = mallinfo2().arena;
            (round == 0 ? first_build : builds.emplace_back()) = ms(t1 - t0);
            teardowns.push_back(ms(t3 - t2));
            released.push_back(static_cast<double>(arena_before - arena_after) / 1048576.0);
        }
        std::printf("%s n=%zu first_build %.3f warm_build %.3f teardown %.3f released_mb %.1f\n",
                    kind,
                    n,
                    first_build,
                    median(builds),
                    median(teardowns),
                    median(released));
    }
    return 0;
}
