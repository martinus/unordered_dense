// Lookup cost against table size, for this map, the revision it is measured against, and boost.
//
// The scored benchmark stops at 200000 entries, whose index fits in cache on any machine that runs
// it. This walks the size axis instead, which is where the shape of a design shows: a dense map
// pays one more dependent load per lookup than a flat one, and that costs nothing while the index
// is in L2 and a great deal once it is not. Emits CSV on stdout; scripts/ab/plot.py draws it.
#include <ankerl/unordered_dense.h>
#include <base.h>
#ifdef UDM_AB_HAVE_BOOST
#    include <boost/unordered/unordered_flat_map.hpp>
#endif
#include <bench/workloads.h>
#include <third-party/nanobench.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

// One measurement: build a table of n, then time lookups drawn uniformly from the key set. `hit`
// asks for keys that are there, otherwise for keys that are not; the two differ because a miss
// usually stops at the first group and a hit has to reach the value.
template <typename Map>
auto lookup_ns(std::vector<std::uint64_t> const& keys, bool hit) -> double {
    auto map = Map();
    map.reserve(keys.size());
    for (auto k : keys) {
        map.try_emplace(k, 1);
    }
    // enough to swamp the timer without making a big table take minutes
    auto const lookups = std::size_t{2000000};
    auto rng = ankerl::nanobench::Rng(5);
    auto acc = std::size_t{};
    auto const t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < lookups; ++i) {
        auto const k = keys[static_cast<std::size_t>(((rng() >> 32U) * keys.size()) >> 32U)];
        acc += map.count(hit ? k : (k ^ 1U));
    }
    auto const ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
    ankerl::nanobench::doNotOptimizeAway(acc);
    return ns / static_cast<double>(lookups);
}

using main_map = udmbase::unordered_dense::map<std::uint64_t, std::size_t>;
using this_map = ankerl::unordered_dense::map<std::uint64_t, std::size_t>;
#ifdef UDM_AB_HAVE_BOOST
using boost_map = boost::unordered_flat_map<std::uint64_t, std::size_t, ankerl::unordered_dense::hash<std::uint64_t>>;
#endif

} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    auto const max_shift = argc > 1 ? std::strtoul(argv[1], nullptr, 10) : 23U;
    std::printf("entries,map,hit_ns,miss_ns\n");
    for (auto shift = 4U; shift <= max_shift; ++shift) {
        auto const n = std::size_t{1} << shift;
        auto keys = std::vector<std::uint64_t>();
        keys.reserve(n);
        auto rng = ankerl::nanobench::Rng(1);
        while (keys.size() < n) {
            keys.push_back(rng() | 1U); // odd, so key ^ 1 is never a key and always a miss
        }
        std::printf("%zu,main,%.3f,%.3f\n", n, lookup_ns<main_map>(keys, true), lookup_ns<main_map>(keys, false));
        std::printf("%zu,this,%.3f,%.3f\n", n, lookup_ns<this_map>(keys, true), lookup_ns<this_map>(keys, false));
#ifdef UDM_AB_HAVE_BOOST
        std::printf("%zu,boost,%.3f,%.3f\n", n, lookup_ns<boost_map>(keys, true), lookup_ns<boost_map>(keys, false));
#endif
        std::fflush(stdout);
    }
    return 0;
}
