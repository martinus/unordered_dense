// What move_home() is worth, one map per binary.
//
//   scripts/ab/move_home.sh <miss|hit|round> <entries> <turnovers> <writing hits per round> <reps>
//
// move_home() only ever runs on a hit inside a path that writes -- try_emplace, operator[], insert
// -- so a table has to be churned *with* writing lookups for it to have done anything. Build, churn
// `turnovers` times through with `hits` writing lookups per round, then time the thing asked for.
//
// **`hits` of 0 is the control and it matters.** With no writing lookups move_home() never fires, so
// the two binaries must measure identically; anything they differ by there is code layout between
// them and has to be subtracted from what they differ by at `hits` of 1. Measured 2026-09-07 the
// control is 0.997 to 1.010 at 840000 and 3.36M entries and as far off as 0.951 at 52363, which is
// why the conclusions below are drawn at the larger sizes.
//
// What it says, at load 0.80, ratio of off to move_home so above 1.00 means move_home is faster:
// misses 1.10 at 52363 entries, 1.10 at 838860 and 1.10 at 3355443 -- so **about a tenth of a miss,
// and at every size rather than only in cache**. Hits 0.99 to 1.04, which is nothing. The churn
// round itself 1.00 to 1.01, so it is not paid for on the writing path. Counters at 52363 entries,
// per lookup: branch misses 0.218 to 0.159 and cycles 28.9 to 25.6 with writing hits, and 0.212
// against 0.212 without them.
#include <ankerl/unordered_dense.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#if defined(__GLIBC__)
#    include <malloc.h>
#endif

namespace {
using map_t = ankerl::unordered_dense::map<std::uint64_t, std::uint64_t>;

struct rng {
    std::uint64_t s;
    explicit rng(std::uint64_t seed)
        : s(seed) {}
    auto operator()() -> std::uint64_t {
        s ^= s << 13U;
        s ^= s >> 7U;
        s ^= s << 17U;
        return s;
    }
};
} // namespace

int main(int argc, char** argv) {
#if defined(__GLIBC__)
    mallopt(M_MMAP_THRESHOLD, 64 * 1024 * 1024);
    mallopt(M_TRIM_THRESHOLD, 64 * 1024 * 1024);
#endif
    auto const what = std::string(argc > 1 ? argv[1] : "miss");
    auto const n = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 50000;
    auto const turnovers = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 40;
    auto const hits = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 1;
    auto const reps = argc > 5 ? std::strtoull(argv[5], nullptr, 10) : 30000000;

    auto r = rng(0x853c49e6748fea9bULL);
    auto present = std::vector<std::uint64_t>();
    auto absent = std::vector<std::uint64_t>();
    for (std::size_t i = 0; i < n; ++i) {
        present.push_back(r() >> 1U);
    }
    for (std::size_t i = 0; i < 65536; ++i) {
        absent.push_back(r() | (std::uint64_t{1} << 63U));
    }

    auto m = map_t();
    m.reserve(n);
    for (auto k : present) {
        m.try_emplace(k, 1);
    }

    auto next = std::uint64_t{1} << 40U;
    auto churn = [&](std::size_t rounds) {
        for (std::size_t i = 0; i < rounds; ++i) {
            for (std::size_t h = 0; h < hits; ++h) {
                ++m[present[static_cast<std::size_t>(r() % present.size())]];
            }
            auto const at = static_cast<std::size_t>(r() % present.size());
            m.erase(present[at]);
            present[at] = next++;
            m.try_emplace(present[at], 1);
        }
    };
    churn(turnovers * n);

    auto acc = std::size_t{0};
    // Only the region under test is timed: the churn above is setup, and at a million entries it is
    // forty million rounds, which would otherwise be most of what a whole-process timer sees.
    auto const t0 = std::chrono::steady_clock::now();
    if (what == "round") {
        // the cost side: the churn itself, with move_home firing on every writing hit
        churn(reps);
        acc += m.size();
    } else {
        auto const& pool = what == "hit" ? present : absent;
        auto const sz = pool.size();
        for (std::size_t i = 0; i < reps; ++i) {
            auto const at = static_cast<std::size_t>(((r() >> 32U) * sz) >> 32U);
            acc += m.count(pool[at]);
        }
    }
    auto const ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
    std::printf("%-5s n=%-8llu turn=%-4llu hits=%llu  %8.2f ns/op   load=%.3f acc=%zu\n", what.c_str(),
                static_cast<unsigned long long>(n), static_cast<unsigned long long>(turnovers),
                static_cast<unsigned long long>(hits), ns / static_cast<double>(reps),
                static_cast<double>(m.size()) / static_cast<double>(m.bucket_count()), acc);
}
