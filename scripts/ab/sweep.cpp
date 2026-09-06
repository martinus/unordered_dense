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
#ifdef UDM_AB_HAVE_JAN
#    include <base_jan.h> // a second baseline, in its own namespace; see scripts/ab/README.md
#endif
#ifdef UDM_AB_HAVE_BOOST
#    include <boost/unordered/unordered_flat_map.hpp>
#endif
#include <bench/workloads.h>
#include <third-party/nanobench.h>

#include <chrono>
#include <cmath>
#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
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

// One of these per map, and it must outlive the epochs. Seeding an rng inside the workload replays
// the same sequence of keys -- and of hit-versus-miss decisions -- on every epoch, which a
// TAGE-style predictor learns: measured here, that made 4.8.1's scalar probe look **1.9x faster
// than it is** at load 0.50 and 2.7x at 0.79, while barely moving a group probe, because the whole
// benefit lands on branches only a branchy probe has. It is the mistake CLAUDE.md records having
// fixed once already in the scored find workload, reintroduced in this tool.
template <typename Map>
struct state {
    using key_type = typename Map::key_type;
    std::vector<key_type> present{}; // in the map
    std::vector<key_type> spare{};   // not in the map: an insert takes one and gives back what it erased
    ankerl::nanobench::Rng rng{5};
    ankerl::nanobench::Rng coin{99};
};

// The three key pools come from disjoint ranges of a 64 bit value, so "absent" means absent by
// construction rather than by luck. `key_for` is the scored benchmark's own key source, which
// scrambles an integer through a bijection -- a small sequential integer hashes to a lattice and
// nothing ever collides -- and for `std::string` draws a length from 8 to 135 bytes skewed towards
// short, since one fixed length makes the hash's length dispatch perfectly predictable. It returns a
// reference into a buffer it rewrites on the next call, so these are copies and have to be.
constexpr auto present_range = std::uint64_t{0};
constexpr auto absent_range = std::uint64_t{1} << 63U;
constexpr auto spare_range = std::uint64_t{1} << 62U;

template <typename Map>
auto make_keys(std::uint64_t range, std::size_t n) -> std::vector<typename Map::key_type> {
    auto out = std::vector<typename Map::key_type>();
    out.reserve(n);
    auto r = ankerl::nanobench::Rng(1);
    for (std::size_t i = 0; i < n; ++i) {
        out.emplace_back(workloads::key_for<Map>((r() >> 2U) | range));
    }
    return out;
}

// The misses, shared by every map because nothing writes to them, and identical for all of them
// because a comparison of maps should differ in the map. Built once per size and kept.
template <typename Map>
auto absent_keys(std::size_t n) -> std::vector<typename Map::key_type> const& {
    static auto cache = std::vector<typename Map::key_type>();
    static auto cached_n = std::size_t{0};
    if (cached_n != n) {
        cache = make_keys<Map>(absent_range, n);
        cached_n = n;
    }
    return cache;
}

// Lookups drawn uniformly from the keys in the map, or from a pool that is not in it. `half` decides
// each lookup with a second rng rather than alternating, for the reason the scored find workload
// does: a predictable sequence of hits and misses is learned by the branch predictor and stops
// measuring the branchy part of a probe.
template <typename Map>
auto lookup_ns(Map const& map, state<Map>& st, asking what, std::size_t lookups) -> double {
    auto const& present = st.present;
    auto const& absent = absent_keys<Map>(present.size());
    auto& rng = st.rng;
    auto& coin = st.coin;
    auto acc = std::size_t{};
    auto const t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < lookups; ++i) {
        auto const at = static_cast<std::size_t>(((rng() >> 32U) * present.size()) >> 32U);
        auto const hit = what == asking::hits || (what == asking::half && (coin() & 1U) != 0);
        acc += map.count(hit ? present[at] : absent[at]);
    }
    auto const ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
    ankerl::nanobench::doNotOptimizeAway(acc);
    return ns / static_cast<double>(lookups);
}

// Erase a live key and insert one that is not there, which is the scored churn workload's core: the
// size never changes, so the table neither grows nor shrinks and what is measured is the steady
// state. The erased key goes back into the spare pool and the inserted one takes its place in
// `present`, so both pools stay truthful without building a key inside the timed region -- which for
// a string would measure the allocator.
template <typename Map>
auto churn_ns(Map& map, state<Map>& st, std::size_t ops) -> double {
    auto& present = st.present;
    auto& spare = st.spare;
    auto& rng = st.rng;
    auto const t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < ops; ++i) {
        auto const slot = static_cast<std::size_t>(((rng() >> 32U) * present.size()) >> 32U);
        auto const j = i % spare.size();
        map.erase(present[slot]);
        map.try_emplace(spare[j], 1);
        std::swap(present[slot], spare[j]);
    }
    auto const ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
    return ns / static_cast<double>(ops);
}

// The scored churn's shape -- erase one, look up twice, insert one -- with the hit made through
// operator[] rather than find. Mode 1 has no lookups at all and the scored churn looks up with
// find, and neither can see move_home, which only runs inside a write: a hit found past its home
// group moves home if there is room. The table is not rebuilt between epochs, so it drifts as the
// epochs churn it -- fully at small sizes, where 20000 rounds is many turnovers, and hardly at all
// at a million entries, which is what a table that size sees in the same number of operations.
template <typename Map>
auto churn_writing_ns(Map& map, state<Map>& st, std::size_t ops) -> double {
    auto& present = st.present;
    auto& spare = st.spare;
    auto const& absent = absent_keys<Map>(present.size());
    auto& rng = st.rng;
    auto acc = std::size_t{};
    auto const t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < ops; ++i) {
        auto const a = static_cast<std::size_t>(((rng() >> 32U) * present.size()) >> 32U);
        map[present[a]] += 1; // present: a writing hit
        auto const slot = static_cast<std::size_t>(((rng() >> 32U) * present.size()) >> 32U);
        auto const j = i % spare.size();
        map.erase(present[slot]);
        acc += map.count(absent[a]);
        map.try_emplace(spare[j], 1);
        std::swap(present[slot], spare[j]);
    }
    auto const ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
    ankerl::nanobench::doNotOptimizeAway(acc);
    return ns / static_cast<double>(ops);
}

// The scored insert_erase's shape, with the size pinned rather than left to a random walk: half of
// the operator[] find a key that is there and half insert one, half of the erases remove a key and
// half find nothing. Doing all four every round means the size is invariant by construction --
// letting the halves cancel only on average drains a small table to nothing, which is a segfault
// rather than a measurement. Returns nanoseconds per operator[]-and-erase pair, of which there are
// two per round.
template <typename Map>
auto insert_erase_ns(Map& map, state<Map>& st, std::size_t ops) -> double {
    auto& present = st.present;
    auto& spare = st.spare;
    auto const& absent = absent_keys<Map>(present.size());
    auto& rng = st.rng;
    auto const t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < ops; ++i) {
        auto const a = static_cast<std::size_t>(((rng() >> 32U) * present.size()) >> 32U);
        map[present[a]] = 1; // present: operator[] that finds
        map.erase(absent[a]); // absent: erase that finds nothing
        // Erase before inserting, so the size never rises above n. The other order crosses the
        // growth threshold and one operation pays for rehashing the whole table -- real, but it
        // is the build workload's cost, and amortised over a measurement it swamps everything
        // else: 1219 ns per operation at 64M entries against the 20 the steady state costs.
        auto const b = static_cast<std::size_t>(((rng() >> 32U) * present.size()) >> 32U);
        auto const j = i % spare.size();
        map.erase(present[b]);  // erase that removes
        map[spare[j]] = 1;      // operator[] that inserts
        std::swap(present[b], spare[j]);
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
template <typename... Alternatives>
void measure_point(std::size_t n, std::size_t buckets, std::size_t batch, double targetWidth, bool emit,
                   Alternatives&&... alternatives) {
    auto bench = ankerl::nanobench::Bench();
    bench.batch(static_cast<double>(batch))
        .complexityN(static_cast<double>(n))
        .performanceCounters(false)
        .output(nullptr)
        .targetIntervalWidth(targetWidth)
        .maxEpochs(400);
    // Variadic, so that which maps are in the comparison is decided at the call site by the same
    // #ifdefs that decide whether their headers are there at all. compare() takes the alternatives
    // as name/callable pairs and uses the first as the baseline of every ratio.
    auto const res = bench.compare(std::forward<Alternatives>(alternatives)...);
    // One row per alternative. `relative` inside the section is the ratio against the baseline, and
    // the two bounds are what say whether to believe it; `buckets` is not nanobench's to know, so it
    // is printed around the render rather than through it.
    // A confidence interval for the absolute time as well as for the ratio. It is a different
    // quantity and it is worth being clear which: nanobench's `relative` interval is about the
    // *ratio*, where whatever the machine did to one alternative it did to the other, so drift
    // cancels. This one is about the median epoch of a single alternative, so drift does not cancel
    // -- it is how well this run pinned its own median, not how well the number would reproduce on
    // a different afternoon. Same distribution-free sign test underneath, on the per-epoch times.
    // `detail::` because a CompareResult renders the ratio's interval and not this one; the epochs
    // themselves are public, through `size()` and `get()`.
    auto intervals = std::map<std::string, std::pair<double, double>>();
    for (std::size_t a = 0; a < res.size(); ++a) { // a CompareResult indexes; it is not a range
        auto const& r = res[a].result;
        auto times = std::vector<double>();
        times.reserve(r.size());
        for (std::size_t i = 0; i < r.size(); ++i) {
            times.push_back(r.get(i, ankerl::nanobench::Result::Measure::elapsed) * 1e9 / static_cast<double>(batch));
        }
        intervals[r.config().mBenchmarkName] = ankerl::nanobench::detail::medianInterval(std::move(times), 0.95);
    }

    if (!emit) {
        return; // the warm-up point: run for the machine's sake, report nothing
    }

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
        // Keyed by the alternative's name rather than by position, so nothing here depends on
        // render walking the alternatives in the order compare() was given them.
        auto const found = fields.size() == 8U ? intervals.find(fields[1]) : intervals.end();
        if (found == intervals.end()) {
            continue;
        }
        auto const toNs = [batch](std::string const& seconds) {
            return std::strtod(seconds.c_str(), nullptr) * 1e9 / static_cast<double>(batch);
        };
        auto const& ci = found->second;
        std::printf("%s,%s,%.4f,%.4f,%.4f,%.4f,%s,%s,%s,%s,%zu\n",
                    fields[0].c_str(),
                    fields[1].c_str(),
                    toNs(fields[2]),
                    toNs(fields[3]),
                    ci.first,
                    ci.second,
                    fields[4].c_str(),
                    fields[5].c_str(),
                    fields[6].c_str(),
                    fields[7].c_str(),
                    buckets);
    }
    std::fflush(stdout);
}

} // namespace

// The whole sweep for one key type. `std::string` keys are the same workloads on keys of 8 to 135
// bytes: the hash stops being one multiply and becomes most of a lookup, and the comparison stops
// being one instruction and becomes a memcmp against a pointer the map has to chase.
template <typename Key>
void sweep(unsigned max_shift, unsigned per_octave, std::size_t batch, int mode, double targetWidth) {
    using main_map = udmbase::unordered_dense::map<Key, std::size_t>;
#ifdef UDM_AB_HAVE_JAN
    using jan_map = udmjan::unordered_dense::map<Key, std::size_t>;
#endif
    using this_map = ankerl::unordered_dense::map<Key, std::size_t>;
#ifdef UDM_AB_HAVE_BOOST
    using boost_map = boost::unordered_flat_map<Key, std::size_t, ankerl::unordered_dense::hash<Key>>;
    // The same map with the hash it ships with, because that is what a caller gets by writing
    // boost::unordered_flat_map and not passing a third argument. Every other alternative here is
    // given this map's hash so that the comparison is of indexes; this one is the control that says
    // what the hash choice is worth on its own.
    using boostdef_map = boost::unordered_flat_map<Key, std::size_t>;
#endif

    // The maps are rebuilt from scratch at every sample point rather than grown on from the last
    // one. Carrying them made each map's addresses depend on the whole history of the others'
    // allocations, and an unlucky layout then sat there for a stretch of points: in one run boost
    // measured 4.72 ns at 128 entries where every other run of the same work says 2.4-2.6, and it
    // stayed wrong from 128 up to about 8192. Rebuilding costs about a second across the whole
    // sweep -- the sum of every point's n is ~17M inserts, against ~55 minutes of measuring -- and
    // it buys independence: a bad layout now spoils one point, which reads as a spike a second run
    // does not have, instead of a smooth and plausible stretch of a curve.
    auto m0 = main_map();
    auto m1 = this_map();
    auto s0 = state<main_map>();
    auto s1 = state<this_map>();
#ifdef UDM_AB_HAVE_BOOST
    auto m2 = boost_map();
    auto s2 = state<boost_map>();
    auto m4 = boostdef_map();
    auto s4 = state<boostdef_map>();
#endif
#ifdef UDM_AB_HAVE_JAN
    auto m3 = jan_map();
    auto s3 = state<jan_map>();
#endif

    auto const points = sample_sizes(max_shift, per_octave);
    auto const first = points.front();
    for (auto n : points) {
        // The keys are rebuilt, the rngs are not: `st.rng` and `st.coin` carry on across points and
        // across epochs, which is what stops the epoch replaying its own key sequence.
        // The pools are rebuilt with the map, the rngs are not: `st.rng` and `st.coin` carry on
        // across points and across epochs, which is what stops an epoch replaying its own sequence.
        auto grow = [n](auto& map, auto& st) {
            using map_type = std::decay_t<decltype(map)>;
            map = {};
            st.present = make_keys<map_type>(present_range, n);
            // Enough spares that churn is not re-inserting the same handful of home groups, and few
            // enough that the pools do not dwarf the map they are about.
            st.spare = make_keys<map_type>(spare_range, std::min<std::size_t>(n, 1U << 14U));
            static_cast<void>(absent_keys<map_type>(n));
            for (auto const& k : st.present) {
                map.try_emplace(k, 1);
            }
        };
        grow(m0, s0);
        grow(m1, s1);
#ifdef UDM_AB_HAVE_BOOST
        grow(m2, s2);
        grow(m4, s4);
#endif
#ifdef UDM_AB_HAVE_JAN
        grow(m3, s3);
#endif
        // Modes 3 and 4 exist because the 50% mix is *not* the average of its parts and can order
        // the maps differently from both: measured paired at 33000 entries, this map is fastest on
        // hits (1.25x main) and on misses (1.25x), and 4.8.1 -- last on hits by a wide margin --
        // wins the mix by 1.6%. An unpredictable outcome costs a clean probe a fresh half
        // misprediction per lookup and costs a probe that already mispredicts 0.6 times per hit
        // almost nothing. So a chart of the mix alone cannot be read as "which lookup is faster".
        auto run = [mode, batch](auto& map, auto& st) {
            if (mode == 1) {
                ankerl::nanobench::doNotOptimizeAway(churn_ns(map, st, batch));
            } else if (mode == 2) {
                ankerl::nanobench::doNotOptimizeAway(insert_erase_ns(map, st, batch));
            } else if (mode == 3) {
                ankerl::nanobench::doNotOptimizeAway(lookup_ns(map, st, asking::hits, batch));
            } else if (mode == 4) {
                ankerl::nanobench::doNotOptimizeAway(lookup_ns(map, st, asking::misses, batch));
            } else if (mode == 5) {
                ankerl::nanobench::doNotOptimizeAway(churn_writing_ns(map, st, batch));
            } else {
                ankerl::nanobench::doNotOptimizeAway(lookup_ns(map, st, asking::half, batch));
            }
        };
        // The first sample point of the process reads high and not by a little: measured at 16
        // entries, boost came out 19% above its own value at 17 entries, in a run whose intervals
        // are 1.5% wide, and the two runs before this one disagreed about which map it inflated.
        // Cold caches, a cold allocator and a clock still ramping are all paid by whoever goes
        // first, and pairing cannot cancel that because it is the *point* that is cold rather than
        // one alternative. So the first point is measured twice and the first answer thrown away.
        auto const passes = n == first ? 2 : 1;
        for (auto pass = 0; pass < passes; ++pass) {
            measure_point(n,
                          m1.bucket_count(),
                          batch,
                          targetWidth,
                          pass + 1 == passes,
                          "main",
                          [&] { run(m0, s0); },
                          "this",
                          [&] { run(m1, s1); }
#ifdef UDM_AB_HAVE_JAN
                          ,
                          "jan",
                          [&] { run(m3, s3); }
#endif
#ifdef UDM_AB_HAVE_BOOST
                          ,
                          "boost",
                          [&] { run(m2, s2); },
                          "boostdef",
                          [&] { run(m4, s4); }
#endif
            );
        }
    }
}

auto main(int argc, char** argv) -> int {
    // It matters more since every point rebuilds: the sweep asks for and returns megabytes 193 times
    // over, and kept in the arena those pages are faulted once for the process rather than once per
    // point. glibc only, a no-op elsewhere.
    workloads::tame_allocator();
    auto const max_shift = argc > 1 ? static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10)) : 20U;
    auto const per_octave = argc > 2 ? static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)) : 12U;
    auto const batch = argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 20000UL;
    // 0 find (50% hits), 1 churn, 2 insert-and-erase, 3 find all hits, 4 find all misses,
    // 5 churn with the hit through operator[]
    auto const mode = argc > 4 ? std::atoi(argv[4]) : 0;
    // the interval width to aim for, in log space: 0.02 pins a ratio to about +-1%
    auto const targetWidth = argc > 5 ? std::strtod(argv[5], nullptr) : 0.02;
    // 0 uint64_t keys, 1 std::string keys
    auto const key = argc > 6 ? std::atoi(argv[6]) : 0;

    std::printf("entries,map,ns,ns_min,ns_low,ns_high,relative,rel_low,rel_high,rounds,buckets\n");
    if (key == 1) {
        sweep<std::string>(max_shift, per_octave, batch, mode, targetWidth);
    } else {
        sweep<std::uint64_t>(max_shift, per_octave, batch, mode, targetWidth);
    }
    return 0;
}
