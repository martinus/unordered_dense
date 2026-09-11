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
//   argv: <n> [rounds] [dup-percent]
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

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

    auto acc = std::size_t{0};
    auto const t0 = std::chrono::steady_clock::now();
    for (std::size_t round = 0; round < rounds; ++round) {
        auto container = container_t(source);
        auto map = map_t();
        map.replace(std::move(container));
        acc += map.size();
    }
    auto const el = std::chrono::steady_clock::now() - t0;
    std::printf("%.3f ns/element  n=%zu rounds=%zu dup=%zu%% unique=%zu acc=%zu\n",
                std::chrono::duration<double, std::nano>(el).count() / static_cast<double>(n * rounds),
                n,
                rounds,
                dup_pct,
                distinct.size(),
                acc);
}
