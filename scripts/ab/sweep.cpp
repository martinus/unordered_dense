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
#include <sstream>
#include <string>
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

// One sample point: the maps are all at size n, and nanobench's compare() runs their batches
// interleaved, round after round, in one process. That is the whole point of doing it this way. The
// first version of this file measured main to completion, then this map, then boost, and its ratios
// were not reproducible -- two runs of identical work disagreed by up to 140% at large sizes and by
// tens of percent at small ones, because anything that drifts between the phases (a clock ramp, a
// noisy neighbour, page placement) lands entirely on whichever map was running at the time. A paired
// comparison cancels all of that, and what comes back is an uncertainty about the ratio.
//
// The rounds are chosen by asking for a precision rather than by naming a count, because the count
// a precision needs depends on the machine: at nanobench's default eleven the interval on a ratio
// is about a quarter of it, which cannot tell a 10% difference from none. `entries` rides along in
// complexityN, which is the config field meant for exactly that, so every row says which table it
// came from.
template <typename FMain, typename FThis, typename FBoost>
void measure_point(std::size_t n, std::size_t buckets, std::size_t batch, double targetWidth, FMain&& fm, FThis&& ft,
                   FBoost&& fb) {
    auto bench = ankerl::nanobench::Bench();
    bench.batch(static_cast<double>(batch))
        .complexityN(static_cast<double>(n))
        .performanceCounters(false)
        .output(nullptr)
        .targetIntervalWidth(targetWidth)
        .maxEpochs(400);
#ifdef UDM_AB_HAVE_BOOST
    auto const res = bench.compare("main", fm, "this", ft, "boost", fb);
#else
    static_cast<void>(fb);
    auto const res = bench.compare("main", fm, "this", ft);
#endif
    // One row per alternative. `relative` inside the section is the ratio against the baseline, and
    // the two bounds are what say whether to believe it; `buckets` is not nanobench's to know, so it
    // is printed around the render rather than through it.
    auto row = std::ostringstream();
    // Both estimators, because they answer different questions. The median is what the ratio is
    // built from; the minimum is the least-disturbed epoch, which is the honest answer to "how fast
    // is this operation" and is far steadier run to run -- a machine that drifts slower can only
    // push a measurement up, never down, so the floor moves much less than the middle.
    ankerl::nanobench::render("{{#alternative}}{{complexityN}},{{name}},{{median(elapsed)}},{{minimum(elapsed)}},"
                              "{{relative}},{{relativeLow}},{{relativeHigh}},{{rounds}}\n{{/alternative}}",
                              res,
                              row);
    // Split into fields rather than slicing by offsets: the elapsed time is rendered in seconds per
    // batch and wants converting, and the bucket count is not nanobench's to know, so both are done
    // here. Reassembling from a vector is what a first attempt at this by string offsets got wrong,
    // silently, by one comma.
    auto line = std::string();
    auto lines = std::istringstream(row.str());
    while (std::getline(lines, line)) {
        auto fields = std::vector<std::string>();
        auto field = std::string();
        auto cells = std::istringstream(line);
        while (std::getline(cells, field, ',')) {
            fields.push_back(field);
        }
        if (fields.size() != 8U) {
            continue;
        }
        auto const toNs = [batch](std::string const& seconds) {
            return std::strtod(seconds.c_str(), nullptr) * 1e9 / static_cast<double>(batch);
        };
        std::printf("%s,%s,%.4f,%.4f,%s,%s,%s,%s,%zu\n",
                    fields[0].c_str(),
                    fields[1].c_str(),
                    toNs(fields[2]),
                    toNs(fields[3]),
                    fields[4].c_str(),
                    fields[5].c_str(),
                    fields[6].c_str(),
                    fields[7].c_str(),
                    buckets);
    }
    std::fflush(stdout);
}

} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    auto const max_shift = argc > 1 ? static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10)) : 20U;
    auto const per_octave = argc > 2 ? static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)) : 12U;
    auto const batch = argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 20000UL;
    // 0 a random find with a 50% hit rate, 1 churn at a fixed size, 2 insert and erase
    auto const mode = argc > 4 ? std::atoi(argv[4]) : 0;
    // the interval width to aim for, in log space: 0.02 pins a ratio to about +-1%
    auto const targetWidth = argc > 5 ? std::strtod(argv[5], nullptr) : 0.02;

    using main_map = udmbase::unordered_dense::map<std::uint64_t, std::size_t>;
    using this_map = ankerl::unordered_dense::map<std::uint64_t, std::size_t>;
#ifdef UDM_AB_HAVE_BOOST
    using boost_map = boost::unordered_flat_map<std::uint64_t, std::size_t, ankerl::unordered_dense::hash<std::uint64_t>>;
#else
    using boost_map = this_map;
#endif

    // The maps are carried together and grown together, so that at every sample point each holds n
    // keys and the comparison is of the maps rather than of what else was happening.
    auto m0 = main_map();
    auto m1 = this_map();
    auto m2 = boost_map();
    auto k0 = std::vector<std::uint64_t>();
    auto k1 = std::vector<std::uint64_t>();
    auto k2 = std::vector<std::uint64_t>();
    auto n0 = (std::uint64_t{1} << 40U) | 1U;
    auto n1 = n0;
    auto n2 = n0;

    std::printf("entries,map,ns,ns_min,relative,rel_low,rel_high,rounds,buckets\n");
    for (auto n : sample_sizes(max_shift, per_octave)) {
        auto grow = [n](auto& map, auto& keys) {
            auto r = ankerl::nanobench::Rng(1);
            while (keys.size() < n) {
                auto const key = (r() >> 1U) | 1U;
                if (map.try_emplace(key, 1).second) {
                    keys.push_back(key);
                }
            }
        };
        grow(m0, k0);
        grow(m1, k1);
        grow(m2, k2);
        auto run = [mode, batch](auto& map, auto& keys, std::uint64_t& next) {
            if (mode == 1) {
                ankerl::nanobench::doNotOptimizeAway(churn_ns(map, keys, next, batch));
            } else if (mode == 2) {
                ankerl::nanobench::doNotOptimizeAway(insert_erase_ns(map, keys, next, batch));
            } else {
                ankerl::nanobench::doNotOptimizeAway(lookup_ns(map, keys, asking::half, batch));
            }
        };
        measure_point(
            n,
            m1.bucket_count(),
            batch,
            targetWidth,
            [&] { run(m0, k0, n0); },
            [&] { run(m1, k1, n1); },
            [&] { run(m2, k2, n2); });
    }
    return 0;
}
