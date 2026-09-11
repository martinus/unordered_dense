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
// The modes come in two families. These start at the block's first line, which is what
// prefetch_block does for a caller that has not touched the group at all:
//
//   none   no prefetch at all, for scale
//   one    p only -- if this ties with `two`, the hardware is fetching the rest
//   two    first line and last: p and p + 87, which is what the header did until #250
//   next   what the header does now: p and p + 64, the first line and the one after it
//   three  p, p + 64 and p + 87
//
// These skip it, which is what prefetch_index does -- the probe is reading the fingerprints out of
// that line as the prefetch is issued (#252):
//
//   pair      p + 64 and p + size - 1, which is what prefetch_index has always asked for
//   step      p + 64, p + 128, ... -- consecutive lines, the shape #250 gave prefetch_block
//   steptail  step plus p + size - 1
//
// For the 88 byte block `pair` and `steptail` are the same two addresses and every line is covered
// either way; `step` is one prefetch and gives up the third line, which holds the top index entries.
// For the 152 byte block of `group_big` all three differ and only `steptail` covers every line: the
// block spans four lines when p % 64 > 40, `pair` misses the *middle* one (up to eight of the
// sixteen value indices) and `step` misses the last (at most three of them).
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
//   argv: <none|one|two|next|three|pair|step|steptail> <full|probe> <blocks> [reps] [group|big]
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

// The shape of bucket_type::group plus its indices, which is what a block is, and the same for
// group_big. Spelled out rather than reached through the header so that this file measures the
// layout and not the map.
template <typename ValueIdx>
struct basic_block {
    std::array<std::uint8_t, 16> m_fingerprints;
    std::array<std::uint8_t, 8> m_overflows;
    std::array<ValueIdx, 16> m_index;
};
using block = basic_block<std::uint32_t>;
using block_big = basic_block<std::uint64_t>;
static_assert(sizeof(block) == 88, "this benchmark is about a block spanning three cache lines");
static_assert(sizeof(block_big) == 152, "and group_big's, which spans four");

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

// What to prefetch, and what to read, are template parameters rather than strings tested per
// iteration: the dispatch is a std::string compare and it lands *inside* the measured loop, which
// is a cost that differs per mode. Two spellings of the same pair of addresses -- `pair` and
// `steptail` on the 88 byte block -- read 14.11 and 13.43 ns/block when the loop chose between them
// at run time, so the harness was reporting its own dispatch as a 4.8% difference.
enum class mode { none, one, two, next, three, pair, step, steptail, twoafter };

template <typename Block, mode How, bool Full>
void run(std::size_t n, std::size_t reps, std::string const& label) {
    auto blocks = std::vector<Block>(n);
    auto r = rng(1);
    for (auto& b : blocks) {
        for (auto& f : b.m_fingerprints) {
            f = static_cast<std::uint8_t>(r());
        }
        for (auto& o : b.m_overflows) {
            o = static_cast<std::uint8_t>(r());
        }
        for (auto& i : b.m_index) {
            i = static_cast<typename decltype(b.m_index)::value_type>(r());
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
        if constexpr (How == mode::pair || How == mode::step || How == mode::steptail || How == mode::twoafter) {
            // The prefetch_index family: the first line is being read right now, so it is skipped.
            if constexpr (How == mode::pair) {
                ANKERL_UNORDERED_DENSE_PREFETCH(p + 64);
            } else if constexpr (How == mode::twoafter) {
                ANKERL_UNORDERED_DENSE_PREFETCH(p + 64);
                ANKERL_UNORDERED_DENSE_PREFETCH(p + (sizeof(Block) > 128 ? 128 : sizeof(Block) - 1));
            } else {
                for (std::size_t off = 64; off < sizeof(Block); off += 64) {
                    ANKERL_UNORDERED_DENSE_PREFETCH(p + off);
                }
            }
            if constexpr (How != mode::step && How != mode::twoafter) {
                ANKERL_UNORDERED_DENSE_PREFETCH(p + sizeof(Block) - 1);
            }
        } else {
            ANKERL_UNORDERED_DENSE_PREFETCH(p);
            if constexpr (How == mode::next || How == mode::three) {
                ANKERL_UNORDERED_DENSE_PREFETCH(p + 64);
            }
            if constexpr (How != mode::one && How != mode::next) {
                ANKERL_UNORDERED_DENSE_PREFETCH(p + sizeof(Block) - 1);
            }
        }
    };

    auto acc = std::uint64_t{0};
    auto const t0 = std::chrono::steady_clock::now();
    if constexpr (How != mode::none) {
        for (std::size_t i = 0; i < depth && i < reps; ++i) {
            touch(order[i]);
        }
    }
    for (std::size_t i = 0; i < reps; ++i) {
        if constexpr (How != mode::none) {
            if (i + depth < reps) {
                touch(order[i + depth]);
            }
        }
        auto const& b = blocks[order[i]];
        if constexpr (Full) {
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
    std::printf("%.3f ns/block  %s bytes=%zu blocks=%zu reps=%zu acc=%llu\n",
                std::chrono::duration<double, std::nano>(el).count() / static_cast<double>(reps),
                label.c_str(),
                sizeof(Block),
                n,
                reps,
                static_cast<unsigned long long>(acc));
}

template <typename Block, mode How>
void run_shape(bool full, std::size_t n, std::size_t reps, std::string const& label) {
    if (full) {
        run<Block, How, true>(n, reps, label);
    } else {
        run<Block, How, false>(n, reps, label);
    }
}

template <typename Block>
auto run_mode(std::string const& how, bool full, std::size_t n, std::size_t reps, std::string const& label) -> bool {
    if (how == "none") {
        run_shape<Block, mode::none>(full, n, reps, label);
    } else if (how == "one") {
        run_shape<Block, mode::one>(full, n, reps, label);
    } else if (how == "two") {
        run_shape<Block, mode::two>(full, n, reps, label);
    } else if (how == "next") {
        run_shape<Block, mode::next>(full, n, reps, label);
    } else if (how == "three") {
        run_shape<Block, mode::three>(full, n, reps, label);
    } else if (how == "pair") {
        run_shape<Block, mode::pair>(full, n, reps, label);
    } else if (how == "step") {
        run_shape<Block, mode::step>(full, n, reps, label);
    } else if (how == "steptail") {
        run_shape<Block, mode::steptail>(full, n, reps, label);
    } else if (how == "twoafter") {
        run_shape<Block, mode::twoafter>(full, n, reps, label);
    } else {
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    workloads::tame_allocator();
    auto const how = std::string(argc > 1 ? argv[1] : "two");
    auto const shape = std::string(argc > 2 ? argv[2] : "probe");
    auto const n = static_cast<std::size_t>(argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 2000000);
    auto const reps = static_cast<std::size_t>(argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 20000000);
    auto const which = std::string(argc > 5 ? argv[5] : "group");

    auto const full = shape == "full";
    auto const label = how + " " + shape + " " + which;
    auto const ok = which == "big" ? run_mode<block_big>(how, full, n, reps, label) : run_mode<block>(how, full, n, reps, label);
    if (!ok) {
        std::fprintf(stderr, "unknown mode %s\n", how.c_str());
        return 1;
    }
}
