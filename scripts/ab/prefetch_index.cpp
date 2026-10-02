// Which lines should prefetch_index() name for the default 88 byte block? (2026-10-02)
//
// The 2026-09-07 SSE probe audit kept both `p + 64` and `p + 87` "because the gcc gain is larger
// than the clang cost", but its own table read the other way: dropping one line won 6-11% under
// clang and lost 1.3-1.9% under gcc, one run per cell. This re-takes it the way CLAUDE.md asks for
// anything this size: one header per binary (prefetch_index.sh patches copies of the header), both
// compilers, sizes across the cache boundary, rounds rotated, medians.
//
//   prefetch_index <hit64|hitstr|miss64> <entries> [calls]
//
// The loop is the scored benchmark's own find_all over a lookup_table, a million lookups per call,
// so a hit also reads the key array; that cost is the same for every variant.
#define ANKERL_NANOBENCH_IMPLEMENT // one translation unit, so the Rng the workloads use is defined here
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

template <typename Map, bool Hits>
auto run(std::size_t n, std::size_t calls) -> double {
    auto t = workloads::lookup_table<Map>(n);
    auto acc = std::size_t{0};
    acc += workloads::find_all<Hits>(&t); // warm-up, untimed
    auto const t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < calls; ++i) {
        acc += workloads::find_all<Hits>(&t);
    }
    auto const t1 = std::chrono::steady_clock::now();
    ankerl::nanobench::doNotOptimizeAway(acc);
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / (1e6 * static_cast<double>(calls));
}

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <hit64|hitstr|miss64> <entries> [calls]\n", argv[0]);
        return 1;
    }
    auto const what = std::string(argv[1]);
    auto const n = static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10));
    auto const calls = argc > 3 ? static_cast<std::size_t>(std::strtoull(argv[3], nullptr, 10)) : 20;
    using m64 = ankerl::unordered_dense::map<std::uint64_t, std::size_t>;
    using mstr = ankerl::unordered_dense::map<std::string, std::size_t>;
    auto ns = 0.0;
    if (what == "hit64") {
        ns = run<m64, true>(n, calls);
    } else if (what == "miss64") {
        ns = run<m64, false>(n, calls);
    } else if (what == "hitstr") {
        ns = run<mstr, true>(n, calls);
    } else {
        std::fprintf(stderr, "unknown workload %s\n", what.c_str());
        return 2;
    }
    std::printf("%.4f\n", ns);
    return 0;
}
