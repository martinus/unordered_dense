// Lookup cost against table size, for this map, the revision it is measured against, and boost.
//
// The scored benchmark stops at 200000 entries, whose index fits in cache on any machine that runs
// it. This walks the size axis instead, which is where the shape of a design shows: a dense map
// pays one more dependent load per lookup than a flat one, and that costs nothing while the index
// is in L2 and a great deal once it is not.
//
// Two things make the resizes visible. Nothing is reserved, so the table grows on its own and its
// load factor sweeps from about a half up to the maximum and drops again at every doubling, which
// is a sawtooth in the lookup cost rather than a smooth curve. And the sampling is many points per
// octave instead of one, because a sawtooth sampled once per octave is a straight line. The map is
// grown *through* the sample points rather than rebuilt at each, so the whole sweep costs one
// build rather than one per point.
//
// Emits CSV on stdout; scripts/ab/plot.py draws it.
#include <ankerl/unordered_dense.h>
#include <base.h>
#ifdef UDM_AB_HAVE_BOOST
#    include <boost/unordered/unordered_flat_map.hpp>
#endif
#include <bench/workloads.h>
#include <third-party/nanobench.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

auto sample_sizes(unsigned max_shift, unsigned per_octave) -> std::vector<std::size_t> {
    auto out = std::vector<std::size_t>();
    auto const first = 4U * per_octave;
    for (auto i = first; i <= max_shift * per_octave; ++i) {
        auto const n = static_cast<std::size_t>(std::pow(2.0, static_cast<double>(i) / per_octave) + 0.5);
        if (out.empty() || n > out.back()) {
            out.push_back(n);
        }
    }
    return out;
}

// What a lookup asks for: only keys that are there, only keys that are not, or the realistic mix.
enum class asking { hits, misses, half };

// Lookups drawn uniformly from the keys inserted so far. Every key is odd, so key ^ 1 is never
// present, which is how a miss is made without changing where in the table it lands. `half` decides
// each lookup with a second rng rather than alternating, for the reason the scored find workload
// does: a predictable sequence of hits and misses is learned by the branch predictor and stops
// measuring the branchy part of a probe.
template <typename Map>
auto lookup_ns(Map const& map, std::vector<std::uint64_t> const& keys, asking what, std::size_t lookups) -> double {
    auto rng = ankerl::nanobench::Rng(5);
    auto coin = ankerl::nanobench::Rng(99);
    auto acc = std::size_t{};
    auto const t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < lookups; ++i) {
        auto const k = keys[static_cast<std::size_t>(((rng() >> 32U) * keys.size()) >> 32U)];
        auto const hit = what == asking::hits || (what == asking::half && (coin() & 1U) != 0);
        acc += map.count(hit ? k : (k ^ 1U));
    }
    auto const ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
    ankerl::nanobench::doNotOptimizeAway(acc);
    return ns / static_cast<double>(lookups);
}

// Grow one map through every sample point, measuring at each. Nothing is reserved on purpose.
template <typename Map>
void sweep(char const* who, std::vector<std::size_t> const& sizes, std::size_t lookups) {
    auto map = Map();
    auto keys = std::vector<std::uint64_t>();
    auto rng = ankerl::nanobench::Rng(1);
    for (auto n : sizes) {
        while (keys.size() < n) {
            auto const k = rng() | 1U;
            if (map.try_emplace(k, 1).second) {
                keys.push_back(k);
            }
        }
        std::printf("%zu,%s,%.3f,%.3f,%.3f,%zu\n",
                    n,
                    who,
                    lookup_ns(map, keys, asking::hits, lookups),
                    lookup_ns(map, keys, asking::misses, lookups),
                    lookup_ns(map, keys, asking::half, lookups),
                    map.bucket_count());
        std::fflush(stdout);
    }
}

} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    auto const max_shift = argc > 1 ? static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10)) : 23U;
    auto const per_octave = argc > 2 ? static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)) : 12U;
    auto const lookups = argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 300000UL;
    auto const sizes = sample_sizes(max_shift, per_octave);
    std::printf("entries,map,hit_ns,miss_ns,half_ns,buckets\n");
    sweep<udmbase::unordered_dense::map<std::uint64_t, std::size_t>>("main", sizes, lookups);
    sweep<ankerl::unordered_dense::map<std::uint64_t, std::size_t>>("this", sizes, lookups);
#ifdef UDM_AB_HAVE_BOOST
    sweep<boost::unordered_flat_map<std::uint64_t, std::size_t, ankerl::unordered_dense::hash<std::uint64_t>>>(
        "boost", sizes, lookups);
#endif
    return 0;
}
