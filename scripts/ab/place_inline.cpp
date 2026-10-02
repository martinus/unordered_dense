// Is the always_inline on place_element_at() still worth what it costs? (2026-10-02)
//
// The attribute's evidence (the comment above place_element_at) was taken on do_place_element, in a
// translation unit holding one map, building from empty: 14 to 20% slower without it. Since #321 the
// placement is place_element_at, reached from the inlined home-group miss and from inside the noinline
// find_or_place_miss, so the old number may not describe today's shape. This is the same experiment
// on today's header: one map per translation unit, a build from empty, timed and counted.
//
//   place_inline <u64|str> <entries> [builds]
#define ANKERL_NANOBENCH_IMPLEMENT // one translation unit, so the Rng the workloads use is defined here
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
template <typename Map, typename Make>
auto run(std::size_t n, std::size_t builds, Make make) -> double {
    auto keys = std::vector<typename Map::key_type>();
    auto rng = ankerl::nanobench::Rng(123);
    for (std::size_t i = 0; i < n; ++i) {
        keys.push_back(make(rng() >> 1U));
    }
    auto acc = std::size_t{0};
    auto best = 1e300;
    for (std::size_t b = 0; b < builds + 1; ++b) { // the first build is a warm-up
        auto const t0 = std::chrono::steady_clock::now();
        {
            auto m = Map();
            for (auto const& k : keys) {
                m.try_emplace(k, b);
            }
            acc += m.size();
        }
        auto const ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
        if (b > 0 && ns < best) {
            best = ns;
        }
    }
    ankerl::nanobench::doNotOptimizeAway(acc);
    return best / static_cast<double>(n);
}
} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    auto const what = std::string(argc > 1 ? argv[1] : "u64");
    auto const n = static_cast<std::size_t>(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 200000);
    auto const builds = static_cast<std::size_t>(argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 20);
    auto ns = 0.0;
    if (what == "u64") {
        ns = run<ankerl::unordered_dense::map<std::uint64_t, std::uint64_t>>(n, builds, [](std::uint64_t v) {
            return v;
        });
    } else {
        ns = run<ankerl::unordered_dense::map<std::string, std::uint64_t>>(n, builds, [](std::uint64_t v) {
            auto s = std::string(8 + (v % 24), 'x');
            std::memcpy(s.data(), &v, sizeof(v));
            return s;
        });
    }
    std::printf("%.4f\n", ns);
    return 0;
}
