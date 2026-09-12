// Which cache lines of a block should a prefetch name?
//
// A block is 88 bytes -- 152 for `group_big` -- at eight byte alignment, so it sits at base + 88i
// and `88 % 64 == 24` with `gcd(24, 64) == 8`: `p % 64` cycles through {0, 8, ... 56} whatever the
// base is, and the values above 40 -- a quarter of all blocks -- make the block reach one line
// further than the rest. Which line goes unnamed depends on what is asked for, and whether that
// costs anything is a different question from whether it is true, because a CPU's adjacent-line and
// stream prefetchers may fetch it anyway.
//
// This isolates it: an array of blocks far larger than the last level cache, a random walk over it
// with the same sixteen-deep lookahead the map's pipelined loops use, and the block read at the far
// end. The modes come in two families. These start at the block's first line, which is what
// `prefetch_block` does for a caller that has not touched the group at all:
//
//   none      no prefetch at all, for scale -- and the only mode without the lookahead bookkeeping,
//             so it is the scale line rather than a term in any comparison
//   one       p only -- if this ties with `two`, the hardware is fetching the rest
//   two       first line and last: p and p + size - 1, which is what the header did until #250
//   next      **what prefetch_block ships**: p and p + 64, the first line and the one after it
//   three     p, p + 64 and p + size - 1
//   stepfull  p, p + 64, p + 128, ... -- every line, which is what prefetch_block emitted for
//             `group_big` until #252 gave its loop the same two-line clamp the 88 byte block had
//
// These skip the first line, which is what `prefetch_index` does -- the probe is reading the
// fingerprints out of it as the prefetch is issued:
//
//   pair      p + 64 and p + size - 1: **what prefetch_index ships for `group`**, and what it asked
//             for on both types until #252
//   step      p + 64 and p + 128: **what prefetch_index ships for `group_big`**
//   steptail  step plus p + size - 1, i.e. every line the block reaches
//
// On the 88 byte block `pair` and `steptail` name the same two addresses, which makes them a useful
// self-check on the harness rather than two results. On the 152 byte block all three differ and only
// `steptail` covers every line: the block reaches four when `p % 64 > 40`, `pair` misses the middle
// one (eight of the sixteen value indices) and `step` misses the last (at most three of them).
//
// The read shape matters as much as the prefetch, because a sequential sweep of the block trains the
// hardware prefetcher and a probe does not do that:
//
//   full   every byte, the way a rehash writes one
//   probe  sixteen fingerprints, one overflow counter and one index -- what a lookup reads
//
// Numbers from two binaries of this file are not comparable with each other: each mode and shape is
// its own instantiation, so a build's layout shifts everything by a percent or two. Compare modes
// within one binary, interleaved, as scripts/ab/prefetch_lines.sh does.
//
//   argv: <none|one|two|next|three|stepfull|pair|step|steptail> <full|probe> <blocks> [reps] [group|big]
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
// layout and not the map -- and asserted against the header's own block, so that a layout change
// there does not leave this file quietly measuring the old one.
template <typename ValueIdx>
struct basic_block {
    std::array<std::uint8_t, 16> m_fingerprints;
    std::array<std::uint8_t, 8> m_overflows;
    std::array<ValueIdx, 16> m_index;
};
using block = basic_block<std::uint32_t>;
using block_big = basic_block<std::size_t>;

template <typename Group>
using header_block = typename ankerl::unordered_dense::detail::group_storage<Group, std::allocator<char>>::block;
static_assert(sizeof(block) == sizeof(header_block<ankerl::unordered_dense::bucket_type::group>));
static_assert(sizeof(block_big) == sizeof(header_block<ankerl::unordered_dense::bucket_type::group_big>));
static_assert(sizeof(block) == 88, "this benchmark is about a block spanning three cache lines");
static_assert(sizeof(void*) != 8 || sizeof(block_big) == 152, "and group_big's, which spans four where a size_t is eight bytes");

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
enum class mode { none, one, two, next, three, stepfull, pair, step, steptail };

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
        constexpr auto last = sizeof(Block) - 1;
        if constexpr (How == mode::one) {
            ANKERL_UNORDERED_DENSE_PREFETCH(p);
        } else if constexpr (How == mode::two) {
            ANKERL_UNORDERED_DENSE_PREFETCH(p);
            ANKERL_UNORDERED_DENSE_PREFETCH(p + last);
        } else if constexpr (How == mode::next) {
            ANKERL_UNORDERED_DENSE_PREFETCH(p);
            ANKERL_UNORDERED_DENSE_PREFETCH(p + 64);
        } else if constexpr (How == mode::three) {
            ANKERL_UNORDERED_DENSE_PREFETCH(p);
            ANKERL_UNORDERED_DENSE_PREFETCH(p + 64);
            ANKERL_UNORDERED_DENSE_PREFETCH(p + last);
        } else if constexpr (How == mode::stepfull) {
            for (std::size_t off = 0; off < sizeof(Block); off += 64) {
                ANKERL_UNORDERED_DENSE_PREFETCH(p + off);
            }
        } else if constexpr (How == mode::pair) {
            ANKERL_UNORDERED_DENSE_PREFETCH(p + 64);
            ANKERL_UNORDERED_DENSE_PREFETCH(p + last);
        } else if constexpr (How == mode::step) {
            for (std::size_t off = 64; off < sizeof(Block); off += 64) {
                ANKERL_UNORDERED_DENSE_PREFETCH(p + off);
            }
        } else if constexpr (How == mode::steptail) {
            for (std::size_t off = 64; off < sizeof(Block); off += 64) {
                ANKERL_UNORDERED_DENSE_PREFETCH(p + off);
            }
            ANKERL_UNORDERED_DENSE_PREFETCH(p + last);
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
    } else if (how == "stepfull") {
        run_shape<Block, mode::stepfull>(full, n, reps, label);
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
