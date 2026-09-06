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

// Erase a live key and insert a fresh one, which is the scored churn workload's core: the size
// never changes, so the table neither grows nor shrinks and what is measured is the steady state.
// Returns nanoseconds per erase-and-insert pair.
template <typename Map>
auto churn_ns(Map& map, std::vector<std::uint64_t>& keys, std::uint64_t& next, std::size_t ops) -> double {
    auto rng = ankerl::nanobench::Rng(7);
    auto const t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < ops; ++i) {
        auto const slot = static_cast<std::size_t>(((rng() >> 32U) * keys.size()) >> 32U);
        map.erase(keys[slot]);
        auto const fresh = (next += 2); // odd throughout, so key ^ 1 is still never present
        map.try_emplace(fresh, 1);
        keys[slot] = fresh;
    }
    auto const ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
    return ns / static_cast<double>(ops);
}

// The scored insert_erase's shape, with the size pinned rather than left to a random walk: half of
// the operator[] find a key that is there and half insert one, half of the erases remove a key and
// half find nothing. Doing all four every round means the size is invariant by construction --
// letting the halves cancel only on average drains a small table to nothing, which is a segfault
// rather than a measurement. Returns nanoseconds per operator[]-and-erase pair, of which there are
// two per round.
template <typename Map>
auto insert_erase_ns(Map& map, std::vector<std::uint64_t>& keys, std::uint64_t& next, std::size_t ops) -> double {
    auto rng = ankerl::nanobench::Rng(11);
    auto const t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < ops; ++i) {
        auto const a = static_cast<std::size_t>(((rng() >> 32U) * keys.size()) >> 32U);
        map[keys[a]] = 1;             // present: operator[] that finds
        map.erase(keys[a] ^ 1U);      // absent: erase that finds nothing
        // Erase before inserting, so the size never rises above n. The other order crosses the
        // growth threshold and one operation pays for rehashing the whole table -- real, but it
        // is the build workload's cost, and amortised over a measurement it swamps everything
        // else: 1219 ns per operation at 64M entries against the 20 the steady state costs.
        auto const b = static_cast<std::size_t>(((rng() >> 32U) * keys.size()) >> 32U);
        map.erase(keys[b]);           // erase that removes
        auto const fresh = (next += 2);
        map[fresh] = 1;               // operator[] that inserts
        keys[b] = fresh;              // and the fresh one takes its place, so the size never moves
    }
    auto const ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
    return ns / static_cast<double>(2 * ops);
}

// Grow one map through every sample point, measuring at each. Nothing is reserved on purpose.
template <typename Map>
void sweep(char const* who, std::vector<std::size_t> const& sizes, std::size_t ops, int mode) {
    auto map = Map();
    auto keys = std::vector<std::uint64_t>();
    auto rng = ankerl::nanobench::Rng(1);
    auto next = std::uint64_t{1} << 40U; // fresh keys, disjoint from the built ones
    for (auto n : sizes) {
        while (keys.size() < n) {
            auto const k = (rng() >> 1U) | 1U;
            if (map.try_emplace(k, 1).second) {
                keys.push_back(k);
            }
        }
        auto value = 0.0;
        if (mode == 1) {
            value = churn_ns(map, keys, next, ops);
        } else if (mode == 2) {
            value = insert_erase_ns(map, keys, next, ops);
        } else {
            value = lookup_ns(map, keys, asking::half, ops);
        }
        std::printf("%zu,%s,%.3f,%zu\n", n, who, value, map.bucket_count());
        std::fflush(stdout);
    }
}

} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    auto const max_shift = argc > 1 ? static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10)) : 23U;
    auto const per_octave = argc > 2 ? static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)) : 12U;
    auto const ops = argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 300000UL;
    // 0 a random find with a 50% hit rate, 1 churn at a fixed size, 2 insert and erase
    auto const mode = argc > 4 ? std::atoi(argv[4]) : 0;
    auto const sizes = sample_sizes(max_shift, per_octave);
    std::printf("entries,map,ns,buckets\n");
    sweep<udmbase::unordered_dense::map<std::uint64_t, std::size_t>>("main", sizes, ops, mode);
    sweep<ankerl::unordered_dense::map<std::uint64_t, std::size_t>>("this", sizes, ops, mode);
#ifdef UDM_AB_HAVE_BOOST
    sweep<boost::unordered_flat_map<std::uint64_t, std::size_t, ankerl::unordered_dense::hash<std::uint64_t>>>(
        "boost", sizes, ops, mode);
#endif
    return 0;
}
