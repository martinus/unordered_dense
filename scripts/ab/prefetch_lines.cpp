// Does a block's middle cache line need its own prefetch?
//
// A block is 88 bytes with 4-byte alignment, so it sits at base + 88i and `88 % 64 == 24` with
// `gcd(24, 64) == 8`: p % 64 walks the whole cycle {0, 24, 48, 8, 32, 56, 16, 40} whatever the base
// is, and the two values above 40 -- exactly a quarter of all blocks -- make the block span *three*
// cache lines. prefetch_block asks for the first and the last, so for that quarter the middle line
// is never requested, and the middle line is where the eight overflow counters and half the
// fingerprints live.
//
// Whether that costs anything is a different question from whether it is true, because a CPU's
// adjacent-line and stream prefetchers may fetch it anyway. This isolates it: an array of blocks far
// larger than the last level cache, a random walk over it with the same sixteen-deep lookahead the
// map's pipelined loops use, and every byte of the block read at the far end.
//
//   none   no prefetch at all, for scale
//   one    p only -- if this ties with `two`, the hardware is fetching the rest
//   two    first line and last: p and p + 87, which is what the header did until #250
//   next   what the header does now: p and p + 64, the first line and the one after it
//   three  p, p + 64 and p + 87
//
// The read shape matters as much as the prefetch, because a sequential sweep of the block trains the
// hardware prefetcher and a probe does not do that:
//
//   full   every byte, the way a rehash writes one
//   probe  sixteen fingerprints, one overflow counter and one index -- what a lookup reads
//
// `two` and `next` differ only for the quarter of blocks that span three lines, where they prefetch
// the same first line and then disagree about the second: `two` takes the last line, `next` takes
// the middle one. For the other three quarters they issue prefetches to exactly the same two lines.
//
//   argv: <none|one|two|next|three> <full|probe> <blocks> [reps]
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

// The shape of bucket_type::group plus its indices, which is what a block is. Spelled out rather
// than reached through the header so that this file measures the layout and not the map.
struct block {
    std::array<std::uint8_t, 16> m_fingerprints;
    std::array<std::uint8_t, 8> m_overflows;
    std::array<std::uint32_t, 16> m_index;
};
static_assert(sizeof(block) == 88, "this benchmark is about a block spanning three cache lines");

constexpr std::size_t depth = 16;

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
    auto const how = std::string(argc > 1 ? argv[1] : "two");
    auto const shape = std::string(argc > 2 ? argv[2] : "probe");
    auto const n = static_cast<std::size_t>(argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 2000000);
    auto const reps = static_cast<std::size_t>(argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 20000000);

    auto blocks = std::vector<block>(n);
    auto r = rng(1);
    for (auto& b : blocks) {
        for (auto& f : b.m_fingerprints) {
            f = static_cast<std::uint8_t>(r());
        }
        for (auto& o : b.m_overflows) {
            o = static_cast<std::uint8_t>(r());
        }
        for (auto& i : b.m_index) {
            i = static_cast<std::uint32_t>(r());
        }
    }

    // The walk is materialised so that generating it is not on the dependency chain -- the point is
    // the block's cache lines, not the random number generator.
    auto order = std::vector<std::uint32_t>(reps);
    for (auto& o : order) {
        o = static_cast<std::uint32_t>(r() % n);
    }

    auto const* base = blocks.data();
    auto touch = [&](std::size_t at) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- byte arithmetic on the block
        auto const* p = reinterpret_cast<char const*>(base + at);
        ANKERL_UNORDERED_DENSE_PREFETCH(p);
        if (how == "one") {
            return;
        }
        if (how == "next" || how == "three") {
            ANKERL_UNORDERED_DENSE_PREFETCH(p + 64);
        }
        if (how != "next") {
            ANKERL_UNORDERED_DENSE_PREFETCH(p + sizeof(block) - 1);
        }
    };

    auto acc = std::uint64_t{0};
    auto const t0 = std::chrono::steady_clock::now();
    if (how != "none") {
        for (std::size_t i = 0; i < depth && i < reps; ++i) {
            touch(order[i]);
        }
    }
    for (std::size_t i = 0; i < reps; ++i) {
        if (how != "none" && i + depth < reps) {
            touch(order[i + depth]);
        }
        auto const& b = blocks[order[i]];
        if (shape == "full") {
            for (auto f : b.m_fingerprints) {
                acc += f;
            }
            for (auto o : b.m_overflows) {
                acc += o;
            }
            for (auto x : b.m_index) {
                acc += x;
            }
        } else {
            // The lane is derived from the block's own contents so that it cannot be computed
            // before the fingerprints arrive -- a probe's index read waits on its compare.
            for (auto f : b.m_fingerprints) {
                acc += f;
            }
            auto const lane = std::size_t{b.m_fingerprints[0]} & 15U;
            acc += b.m_overflows[lane & 7U];
            acc += b.m_index[lane];
        }
    }
    auto const el = std::chrono::steady_clock::now() - t0;
    std::printf("%.3f ns/block  %s %s blocks=%zu reps=%zu acc=%llu\n",
                std::chrono::duration<double, std::nano>(el).count() / static_cast<double>(reps),
                how.c_str(),
                shape.c_str(),
                n,
                reps,
                static_cast<unsigned long long>(acc));
}
