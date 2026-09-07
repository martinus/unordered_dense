// Bucketized placement against sliding-window placement, as a simulation.
//
// The one thing the group index could copy from an ungrouped map -- `indivi::flat_wmap`, which is
// the fastest hit of the eighteen maps `maps.sh` measures -- is *slot-level placement*: a key takes
// the first free slot within sixteen of its home, instead of the first free slot in the group of
// sixteen its home falls in. That should give a shorter displacement distribution, because a group
// being completely full is likelier than no free slot existing anywhere in a sliding window.
//
// It does, and by very little: 1.0396 windows per placement against 1.0481 at load 0.799, so about
// a fifth of an excess that is already under 5%. For calibration, moving displaced entries home is
// worth four times that and is worth a tenth of an in-cache miss and nothing out of cache. This is
// the ceiling on the change before any of its costs are paid -- and its cost is the merged block,
// since sixteen fingerprints starting at an arbitrary slot are not contiguous in an 88 byte block.
//
// No map involved on purpose: the question is about placement, and a simulation answers it in a
// second where building it would take a redesign.
//
//   clang++ -O2 -std=c++17 scripts/ab/placement.cpp -o placement && ./placement 0.799
#include <cstdio>
#include <cstdint>
#include <cstdlib>
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
} // namespace

int main(int argc, char** argv) {
    auto const load = argc > 1 ? std::strtod(argv[1], nullptr) : 0.79;
    constexpr std::size_t slots = 1U << 20U;
    constexpr std::size_t groups = slots / 16;
    auto const n = static_cast<std::size_t>(static_cast<double>(slots) * load);
    auto r = rng();

    // (a) bucketized: home is a group; the first free slot in the group, else the next group on the
    // triangular sequence.
    {
        auto occ = std::vector<std::uint8_t>(groups, 0);
        double windows = 0;
        for (std::size_t i = 0; i < n; ++i) {
            auto g = static_cast<std::size_t>(r() % groups);
            std::size_t d = 0;
            std::size_t visited = 1;
            while (occ[g] == 16) {
                g = (g + ++d) & (groups - 1);
                ++visited;
            }
            ++occ[g];
            windows += static_cast<double>(visited);
        }
        std::printf("load %.3f  bucketized   %.4f windows per placement\n", load, windows / static_cast<double>(n));
    }
    // (b) sliding: home is a slot; the first free slot in [home, home+16), else step on by 16.
    {
        auto occ = std::vector<std::uint8_t>(slots, 0);
        double windows = 0;
        for (std::size_t i = 0; i < n; ++i) {
            auto h = static_cast<std::size_t>(r() % slots);
            std::size_t visited = 1;
            while (true) {
                std::size_t j = 0;
                for (; j < 16; ++j) {
                    if (occ[(h + j) & (slots - 1)] == 0) {
                        break;
                    }
                }
                if (j < 16) {
                    occ[(h + j) & (slots - 1)] = 1;
                    break;
                }
                h = (h + 16) & (slots - 1);
                ++visited;
            }
            windows += static_cast<double>(visited);
        }
        std::printf("load %.3f  sliding      %.4f windows per placement\n", load, windows / static_cast<double>(n));
    }
}
