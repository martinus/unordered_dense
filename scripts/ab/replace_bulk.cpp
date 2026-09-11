// What replace() costs per element, with and without duplicates in the source.
//
// replace() hands the map a whole container and dedups it in place. Before the pipelining landed it
// was the one bulk build path with no lookahead: it hashed, probed and placed one element at a time,
// over a contiguous container -- the shape the lookahead helps most. What had stopped it being
// pipelined is that removing a duplicate pulls `back()` into the hole, and a lookahead has already
// hashed elements near the back.
//
// Sweep `n` as well as `dup-percent`: the pipeline is gated on the container outgrowing cache, and
// on both sides of that gate the answer is different. Build with -DUDM_RB_STR for string keys.
//
//   argv: <n> [rounds] [dup-percent] [warm|cold]
//
// `warm` -- the default -- replaces into the *same* map every round, after one untimed round that
// allocates the index. Without it the timed region contains a fresh index allocation whose cost
// depends on what the allocator did with the container copy just before it, and the result is bimodal
// between repeats: at sixteen to a hundred thousand `uint64_t` elements the same binary reads 4.40,
// 2.85, 2.88, 4.76, 5.19, 2.76 ns per element, which is wide enough to swamp the 5-10% questions this
// is used to answer. `range_insert.cpp` records the same effect at n = 4096. `cold` restores the
// old behaviour, which is the right one for "what does a caller pay end to end" and the wrong one for
// "does the pipeline inside it pay".
//
// What is timed per round either way is `replace()` itself: the index memset, the values move-assign
// and the dedup walk. The per-round **median** is reported, not the mean over rounds, so that one
// round that faulted cannot carry the number.
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
#if defined(UDM_RB_STR)
using key_type = std::string;
#else
using key_type = std::uint64_t;
#endif
using map_t = ankerl::unordered_dense::map<key_type, std::size_t>;
using container_t = typename map_t::value_container_type;

// Not ankerl::nanobench::Rng: that one's constructor is out of line, so using it would make this
// standalone file need test/app/nanobench.cpp linked in, compiled once per header under test.
struct rng {
    std::uint64_t s;
    explicit rng(std::uint64_t seed)
        : s(seed * UINT64_C(0x9E3779B97F4A7C15) | 1U) {}
    auto operator()() -> std::uint64_t {
        s ^= s << 13U;
        s ^= s >> 7U;
        s ^= s << 17U;
        return s;
    }
};
} // namespace

int main(int argc, char** argv) {
    workloads::tame_allocator();
    auto const n = static_cast<std::size_t>(argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1000000);
    auto const rounds = static_cast<std::size_t>(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 10);
    auto const dup_pct = static_cast<std::size_t>(argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 0);
    auto const warm = std::string(argc > 4 ? argv[4] : "warm") != "cold";

    // Built once: `dup_pct` percent of the entries repeat an earlier key, so the dedup path is
    // exercised at a chosen rate rather than never or always.
    auto source = container_t();
    source.reserve(n);
    auto r = rng(1);
    auto distinct = std::vector<std::uint64_t>();
    for (std::size_t i = 0; i < n; ++i) {
        auto const dup = !distinct.empty() && (r() % 100) < dup_pct;
        auto const v = dup ? distinct[static_cast<std::size_t>(r() % distinct.size())] : (r() >> 2U);
        if (!dup) {
            distinct.push_back(v);
        }
        source.emplace_back(workloads::key_for<map_t>(v), i);
    }

    // The per-round copy of `source` is outside the timed region. It looks like noise for a small
    // mapped type and swamps everything for a large one -- at 1032 bytes a value it is 66 MiB of
    // memcpy per round against a call that takes milliseconds, which quietly pulled every ratio
    // towards 1.0 the first time this was used to compare two value sizes.
    auto acc = std::size_t{0};
    auto per_round = std::vector<double>();
    per_round.reserve(rounds);
    auto warm_map = map_t();
    if (warm) {
        // Untimed, and the only round that allocates an index: every later replace() finds one of
        // the right size already there and keeps it.
        auto container = container_t(source);
        warm_map.replace(std::move(container));
    }
    for (std::size_t round = 0; round < rounds; ++round) {
        auto container = container_t(source);
        auto cold_map = map_t();
        auto& map = warm ? warm_map : cold_map;
        if (warm) {
            map.clear(); // frees the values, keeps the index; outside the clock
        }
        auto const t0 = std::chrono::steady_clock::now();
        map.replace(std::move(container));
        auto const elapsed = std::chrono::steady_clock::now() - t0;
        per_round.push_back(std::chrono::duration<double, std::nano>(elapsed).count() / static_cast<double>(n));
        acc += map.size();
    }
    std::sort(per_round.begin(), per_round.end());
    std::printf("%.3f ns/element  n=%zu rounds=%zu dup=%zu%% %s unique=%zu min=%.3f max=%.3f acc=%zu\n",
                per_round[per_round.size() / 2],
                n,
                rounds,
                dup_pct,
                warm ? "warm" : "cold",
                distinct.size(),
                per_round.front(),
                per_round.back(),
                acc);
}
