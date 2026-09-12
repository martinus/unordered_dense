// Paired A/B of the working-tree header against another revision of it, and optionally against
// boost::unordered_flat_map, using nanobench's compare(): the alternatives run interleaved in the
// same slice of time, so machine drift cancels out and the reported interval is about the ratio.
//
// The two headers coexist in one binary because run.sh renames the baseline's namespace and macro
// prefix (ankerl -> udmbase) into base.h. The workloads are those of bench_quick_overall_udm, from
// the header the benchmark itself uses, plus its all-hits and no-hits lookups.
//
// Every size-sensitive workload is measured at several sizes across one octave -- from n to just
// under 2n -- and what is reported is the geometric mean of the per-size ratios.
//
// That is not a refinement, it is the difference between a right answer and a wrong one. A table
// doubles its bucket array at one size and not at another, so its load factor sweeps from about a
// half to the maximum and drops back; the cost of a lookup, an insert and an erase all ride that
// sawtooth. Two indexes that hold different numbers of slots per group double at *different*
// sizes, so a comparison at one fixed size measures wherever that size happens to fall on each of
// their sawtooths. Measured: an eleven-slot group against the shipped sixteen-slot one read 1.384
// on random misses and 1.199 on churn at the single size the score used, and 1.039 and **0.974**
// averaged over the octave -- churn reversed sign. Every headline number of that run was inside
// the sawtooth's own range, which spans 0.79 to 1.33.
//
// The charts have summarised per octave since 2026-09-06 for exactly this reason. The score did
// not, and this is that fix.
//
// How many sizes is a compile-time constant (run.sh -p passes -DUDM_AB_POINTS), because the sizes
// are template arguments. Five averages the sawtooth out; fifty draws it, which is what shows
// where its peak is and how far from the mean the score's own single size sits. What each point
// costs is held flat as the count rises: the lookup workloads search a table that was built once,
// outside the timed region, and the workloads that would otherwise multiply the run by ten -- the
// two that grow a map from empty while operating on it -- keep the five they need, because a cost
// that integrates over every doubling has no sawtooth left to sample. Measured at fifty points,
// per element over one octave: churn64 1.28x cheapest to dearest and rmiss64 1.46x, against 1.02x
// for insert_erase and a monotone 1.14x for find_50 that climbs with the size and has no step over
// 1.3% anywhere -- where rmiss64 steps 32% across the single size at which the array doubles.
#include "base.h"

#include <ankerl/unordered_dense.h>
#include <bench/workloads.h>
#include <third-party/nanobench.h>
#ifdef UDM_AB_HAVE_BOOST
#    include <boost/unordered/unordered_flat_map.hpp>
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using base_u64 = udmbase::unordered_dense::map<uint64_t, size_t>;
using base_str = udmbase::unordered_dense::map<std::string, size_t>;
using base_big = udmbase::unordered_dense::map<uint64_t, workloads::big_value>;
template <typename K, typename V>
using cand_map = ankerl::unordered_dense::map<K, V>;
using cand_u64 = cand_map<uint64_t, size_t>;
using cand_str = cand_map<std::string, size_t>;
using cand_big = cand_map<uint64_t, workloads::big_value>;
#ifdef UDM_AB_HAVE_BOOST
using boost_u64 = boost::unordered_flat_map<uint64_t, size_t, ankerl::unordered_dense::hash<uint64_t>>;
using boost_str = boost::unordered_flat_map<std::string, size_t, ankerl::unordered_dense::hash<std::string>>;
using boost_big = boost::unordered_flat_map<uint64_t, workloads::big_value, ankerl::unordered_dense::hash<uint64_t>>;
#else
using boost_u64 = cand_u64;
using boost_str = cand_str;
using boost_big = cand_big;
#endif

// Workload is a template with a map and a size. The result is whatever it returns;
// doNotOptimizeAway needs it to be a scalar, which is why insert_erase's pair is summed.
template <typename T>
auto scalar(T v) -> T {
    return v;
}

auto scalar(workloads::insert_erase_result r) -> size_t {
    return r.erased + r.size;
}

// How many sizes across one octave, and which of them.
//
// A compile-time constant because every size is a template argument, so run.sh -p rebuilds rather
// than re-runs. The i-th of P points is n * 2^(i/P), which spans exactly one doubling however many
// are asked for -- whereas a table of five scales with only the first three of them used, which is
// what this harness did until 2026-09-12, samples three fifths of an octave and calls it one.
#ifndef UDM_AB_POINTS
#    define UDM_AB_POINTS 5
#endif
constexpr size_t sweep_points = UDM_AB_POINTS;

// 2^(i/P) without <cmath>: std::pow is not constexpr in C++17 and these sizes are template
// arguments. Newton on x^P = 2 from 1 + 1/P, which converges from above for every P; twenty
// iterations are far more than sizes rounded to whole elements can tell apart. Truncating rather
// than rounding is what scripts/ab/maps.cpp's octave_size does, so both harnesses mean the same
// set of sizes by "the octave from n".
constexpr auto ipow(double x, size_t n) -> double {
    auto r = 1.0;
    for (size_t i = 0; i < n; ++i) {
        r *= x;
    }
    return r;
}

constexpr auto root_of_two(size_t p) -> double {
    auto x = 1.0 + 1.0 / static_cast<double>(p);
    for (size_t i = 0; i < 20; ++i) {
        auto const xp = ipow(x, p);
        x -= (xp - 2.0) * x / (static_cast<double>(p) * xp);
    }
    return x;
}

constexpr auto octave_size(size_t n, size_t i, size_t points) -> size_t {
    return static_cast<size_t>(static_cast<double>(n) * ipow(root_of_two(points), i));
}

// What a workload's size means, which is what decides how many sizes it is worth measuring at and
// what the per-point line can say about them.
enum class kind {
    // No bucket array whose load factor cycles: iteration walks the dense value vector and the
    // hash workload has no table at all. One point, and sweeping would only cost time.
    one_size,
    // The map grows from empty to about n while the workload operates on it, so the cost is an
    // integral over every doubling it went through and the load factor it ends at is a small part
    // of it. The five points these get sample the *size* axis, which still moves the ratio between
    // two maps: they are not one point. What they do not need is a finer grid, because there is no
    // cycle under the trend to resolve -- measured at fifty points with this cap lifted,
    // insert_erase's per-element cost moves 1.02x from the cheapest size of an octave to the
    // dearest and find_50's 1.14x monotonically, and neither has a single step over 1.3%, where
    // rmiss64 steps 32% across the one size at which the array doubles. The live size is about
    // half of n for both, so the point's load factor is not the table's and is not printed.
    grows,
    // n is the number of live entries and the payload is n operations: build and churn. Both
    // carry the whole sawtooth -- churn sits at one load for the entire run, and half of a build
    // is spent in its last doubling.
    sized,
    // n is the number of live entries and the payload is a fixed number of lookups against a
    // table built outside the timed region. The sawtooth is at its sharpest here -- 1.46x on a
    // miss, of which 32% is the one step across the doubling -- and a point costs the same at
    // every size. A hit does not ride it: rhit64's largest step is 2.0% and not at the doubling.
    searches
};

constexpr auto points_for(kind k) -> size_t {
    return k == kind::one_size ? 1 : (k == kind::grows ? (std::min)(sweep_points, size_t{5}) : sweep_points);
}

// Where a map's bucket array actually grows, asked of the map rather than assumed.
//
// One octave is one whole turn of the load-factor sawtooth only because the array doubles. That is
// the map's policy and not a law: a growth factor under two -- which this file's notes list as a
// knob -- gives a shorter cycle, and then the geomean over an octave weighs part of that cycle
// twice and part of it once. So insert distinct keys past the top of the sweep and record every
// size at which bucket_count() changes. The ratios between those sizes are the period, printed
// with the run, and anything but two is called out where it cannot be missed.
//
// It also gives the load factor of every point for free, which is what says where on the sawtooth
// a measurement sits -- the thing a single-size number never told anyone.
struct growth {
    struct step {
        size_t at;      // the size at which the array grew
        size_t buckets; // the bucket count it grew to
    };
    std::vector<step> steps;
};

// Below this the steps are the first few allocations, which no workload here measures.
constexpr size_t growth_floor = 1000;

template <typename Map>
auto measure_growth(size_t upto) -> growth {
    auto g = growth();
    auto map = Map();
    auto buckets = map.bucket_count();
    for (size_t i = 0; map.size() < upto; ++i) {
        map[workloads::key_for<Map>(i)] = i;
        if (map.bucket_count() != buckets) {
            buckets = map.bucket_count();
            g.steps.push_back({map.size(), buckets});
        }
    }
    return g;
}

// The load factor of a table holding n entries: n over the bucket count in effect there.
auto load_factor(growth const& g, size_t n) -> double {
    auto buckets = size_t{0};
    for (auto const& s : g.steps) {
        if (s.at <= n) {
            buckets = s.buckets;
        }
    }
    return buckets == 0 ? 0.0 : static_cast<double>(n) / static_cast<double>(buckets);
}

void report_growth(char const* who, growth const& g) {
    auto lo = 1e9;
    auto hi = 0.0;
    auto previous = size_t{0};
    std::printf("# %-5s grows at", who);
    for (auto const& s : g.steps) {
        if (s.at < growth_floor) {
            continue;
        }
        std::printf(" %zu", s.at);
        // The growth factor is the ratio of the bucket counts, which is what it is by definition;
        // the ratio of the sizes at which growth happened only equals it while the maximum load
        // factor is the same on both sides of the step.
        if (previous != 0) {
            auto const r = static_cast<double>(s.buckets) / static_cast<double>(previous);
            lo = (std::min)(lo, r);
            hi = (std::max)(hi, r);
        }
        previous = s.buckets;
    }
    if (lo > hi) {
        std::printf(" (nothing above %zu)\n", growth_floor);
    } else if (lo > 1.99 && hi < 2.01) {
        std::printf("   x%.3f -- one octave is exactly one sawtooth\n", (lo + hi) / 2);
    } else {
        std::printf("   x%.3f..%.3f -- WARNING: the octave is not one whole sawtooth, so the geomean\n"
                    "#       over it weighs part of the cycle twice. Sweep a period, not an octave.\n",
                    lo,
                    hi);
    }
}

// One size point: the ratios are the baseline's time over the other map's, so above 1.00 always
// means the other map is faster -- the orientation the whole harness and summarize.py use.
struct point {
    size_t n;
    double base_ns;
    double cand_ns;
    double cand_ratio;
    double cand_lo;
    double cand_hi;
    double boost_ratio;
};

auto geomean(std::vector<double> const& xs) -> double {
    auto acc = 0.0;
    for (auto x : xs) {
        acc += std::log(x);
    }
    return xs.empty() ? 1.0 : std::exp(acc / static_cast<double>(xs.size()));
}

template <template <typename, size_t> class Workload, typename Base, typename Cand, typename Boost, size_t N>
auto measure_at(char const* name, size_t epochs, bool with_boost) -> point {
    // Output suppressed: fifty points times twenty workloads is a thousand nanobench tables, and
    // the per-point line printed below carries what each of them would have said.
    auto bench = ankerl::nanobench::Bench().title(name).epochs(epochs).performanceCounters(false).output(nullptr);
    // A workload is an object so that one which holds a table builds it here, once, instead of
    // inside what nanobench repeats. The others are empty and cost nothing.
    auto base_w = Workload<Base, N>();
    auto cand_w = Workload<Cand, N>();
    auto boost_w = std::optional<Workload<Boost, N>>();
    if (with_boost) {
        boost_w.emplace();
    }
    auto base = [&] {
        ankerl::nanobench::doNotOptimizeAway(scalar(base_w.run()));
    };
    auto cand = [&] {
        ankerl::nanobench::doNotOptimizeAway(scalar(cand_w.run()));
    };
    auto boost = [&] {
        ankerl::nanobench::doNotOptimizeAway(scalar(boost_w->run()));
    };
    auto const res =
        with_boost ? bench.compare("base", base, "cand", cand, "boost", boost) : bench.compare("base", base, "cand", cand);
    auto const ns = [&](size_t i) {
        return res[i].result.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9;
    };
    // Entry 0 is the baseline, and `relative` is its median over this alternative's: above 1 means
    // the alternative is faster. boost/cand comes out of the two relatives rather than the two
    // medians, which is the same number and keeps one definition of the ratio.
    auto p = point{N, ns(0), ns(1), res[1].relative, res[1].relativeLow, res[1].relativeHigh, 1.0};
    if (with_boost) {
        p.boost_ratio = res[1].relative / res[2].relative;
    }
    return p;
}

// A time in whatever unit reads best: these workloads run from twenty microseconds to fifty
// milliseconds, and one fixed unit makes one end of that unreadable.
void print_time(double ns) {
    if (ns >= 1e6) {
        std::printf("%9.3f ms", ns / 1e6);
    } else if (ns >= 1e3) {
        std::printf("%9.3f us", ns / 1e3);
    } else {
        std::printf("%9.3f ns", ns);
    }
}

// Where n is the number of live entries, what the table's load factor is there. Every other
// workload holds about half of n and its load is not this one, so it is left off the line.
auto load_of(kind k, growth const& g, point const& p) -> double {
    return k == kind::sized || k == kind::searches ? load_factor(g, p.n) : 0.0;
}

void print_load(double load) {
    if (load != 0.0) {
        std::printf(" load %.3f", load);
    }
}

void report(char const* name, std::vector<point> const& pts, bool with_boost, kind k, growth const& g) {
    std::printf("%s\n", name);
    auto cand_ratios = std::vector<double>();
    auto boost_ratios = std::vector<double>();
    for (auto const& p : pts) {
        cand_ratios.push_back(p.cand_ratio);
        boost_ratios.push_back(p.boost_ratio);
        // hashstr has no table, so it has no size to report -- it is given a base size of zero.
        if (p.n == 0) {
            std::printf("  %-11s ", "no table");
        } else {
            std::printf("  n=%-9zu", p.n);
            print_load(load_of(k, g, p));
            std::printf(" ");
        }
        std::printf("base ");
        print_time(p.base_ns);
        std::printf("  cand ");
        print_time(p.cand_ns);
        std::printf("   %6.3f [%.3f .. %.3f]", p.cand_ratio, p.cand_lo, p.cand_hi);
        if (with_boost) {
            std::printf("   boost/cand %6.3f", p.boost_ratio);
        }
        std::printf("\n");
    }
    auto const gm = geomean(cand_ratios);
    auto const gb = geomean(boost_ratios);
    if (pts.size() > 1) {
        std::printf("  octave geomean %.4f", gm);
        if (with_boost) {
            std::printf("   boost/cand %.4f", gb);
        }
        std::printf("   (%zu points, %zu .. %zu)\n", pts.size(), pts.front().n, pts.back().n);
        // The sawtooth itself, which is what more than a handful of points is for: the dearest
        // point of the octave over the cheapest, per element where the payload is n operations and
        // per run where it is a fixed number of lookups. It is a lower bound on the real amplitude
        // and it rises with the point count, because the peak is a size and not an interval --
        // fifty points reach load 0.795 of a maximum of 0.800 and five get no closer than 0.763.
        // The geomean above does not depend on the count that way: an octave is a whole period
        // whatever its phase.
        auto const unit = [k](point const& p) {
            return k == kind::searches ? p.cand_ns : p.cand_ns / static_cast<double>(p.n);
        };
        auto const ends = std::minmax_element(pts.begin(), pts.end(), [&](point const& a, point const& b) {
            return unit(a) < unit(b);
        });
        auto const& cheapest = *ends.first;
        auto const& dearest = *ends.second;
        std::printf("  sawtooth %.3fx  cheapest n=%zu", unit(dearest) / unit(cheapest), cheapest.n);
        print_load(load_of(k, g, cheapest));
        std::printf(", dearest n=%zu", dearest.n);
        print_load(load_of(k, g, dearest));
        std::printf("\n");
    }
    // The machine-readable line summarize.py reads. One per workload, whatever the point count, so
    // a swept run and a single-size run are summarised by the same code.
    std::printf("#ratio %s %.6f %.6f %zu\n\n", name, gm, gb, pts.size());
    std::fflush(stdout);
}

template <template <typename, size_t> class Workload,
          size_t BaseN,
          kind K,
          typename Base,
          typename Cand,
          typename Boost,
          size_t... I>
void compare_seq(char const* name, size_t epochs, bool with_boost, growth const& g, std::index_sequence<I...> /*unused*/) {
    auto pts = std::vector<point>();
    // The sizes are template arguments, so that a workload's default instantiation stays the exact
    // code the scored benchmark compiles.
    (void)std::initializer_list<int>{(
        pts.push_back(measure_at<Workload, Base, Cand, Boost, octave_size(BaseN, I, points_for(K))>(name, epochs, with_boost)),
        0)...};
    report(name, pts, with_boost, K, g);
}

template <template <typename, size_t> class Workload, size_t BaseN, kind K, typename Base, typename Cand, typename Boost>
void compare(char const* name, size_t epochs, bool with_boost, growth const& g) {
    compare_seq<Workload, BaseN, K, Base, Cand, Boost>(name, epochs, with_boost, g, std::make_index_sequence<points_for(K)>{});
}

// The workloads as templates with a map and a size, so that compare() can name them and sweep them.
// Each is instantiated once per octave point; the default size never appears here, it belongs to
// the scored benchmark. run() is not static because the lookup workloads carry the table they
// search, which is built when the object is.
template <typename Map, size_t N>
struct iterate {
    static auto run() {
        return workloads::iterate<Map, N>();
    }
};
template <typename Map, size_t N>
struct insert_erase {
    static auto run() {
        return workloads::insert_erase<Map, N>();
    }
};
template <typename Map, size_t N>
struct build {
    static auto run() {
        return workloads::build<Map, N>();
    }
};
template <typename Map, size_t N>
struct churn {
    static auto run() {
        return workloads::churn<Map, N>();
    }
};
template <typename Map, size_t N>
struct find_50 {
    static auto run() {
        return workloads::find_50<Map, N>();
    }
};
// No table, so no size to sweep: the parameter is there to fit the same machinery.
template <typename Map, size_t>
struct hash_strings {
    static auto run() {
        return workloads::hash_strings<Map>();
    }
};
// The table is a runtime size rather than a template argument: nothing in the timed loop depends
// on it, so making it one would generate fifty copies of identical code per map type.
template <typename Map, size_t N>
struct find_hits {
    workloads::lookup_table<Map> table{N};
    auto run() {
        return workloads::find_all<true>(&table);
    }
};
template <typename Map, size_t N>
struct find_misses {
    workloads::lookup_table<Map> table{N};
    auto run() {
        return workloads::find_all<false>(&table);
    }
};

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 2) {
        std::printf("usage: %s <workload|all> [epochs=12] [boost=0]\n"
                    "workloads: it64 ie64 build64 churn64 find64 itstr iestr buildstr churnstr findstr\n"
                    "           itbig iebig buildbig churnbig findbig (64 byte mapped value)\n"
                    "           (bench_quick_overall_udm)\n"
                    "           rhit64 rmiss64 rhitstr rmissstr (all hits / no hits)\n"
                    "           hashstr (the string hash on its own)\n"
                    "\nThis binary sweeps %zu sizes across one octave and reports the geometric mean\n"
                    "of those ratios. The count is a compile-time constant because the sizes are\n"
                    "template arguments: rebuild with run.sh -p to change it. 1 restores the single\n"
                    "size the score used before 2026-09-07, which is only comparable with itself.\n",
                    argv[0],
                    sweep_points);
        return 1;
    }
    std::string w = argv[1];
    auto epochs = static_cast<size_t>(argc > 2 ? std::atoi(argv[2]) : 12);
    bool boost = argc > 3 && std::atoi(argv[3]) != 0;
    if (argc > 4) {
        std::printf("this binary was built for %zu points -- the count is a compile-time constant now,\n"
                    "rebuild with run.sh -p %s\n",
                    sweep_points,
                    argv[4]);
        return 1;
    }
#ifndef UDM_AB_HAVE_BOOST
    if (boost) {
        std::printf("built without boost, see run.sh\n");
        return 1;
    }
#endif
    auto want = [&](char const* n) {
        return w == "all" || w == n;
    };
    // The probe below allocates and frees a map of the largest size in the run, so it goes through
    // the same allocator settings every workload uses -- otherwise it is the one thing that runs
    // before the first tame_allocator() and it would hand the first workload a heap no other run
    // of this harness had.
    workloads::tame_allocator();
    // Where every map in the run grows, measured before anything is timed. It is what the load
    // factor on each point line is computed from, and it is the check that an octave is one whole
    // turn of the sawtooth rather than an assumption about the growth factor. The integer map
    // answers for all three value types: the bucket array grows on a count, not on a size.
    //
    // The largest size any workload reaches: the builds start at 200000, which is the highest base
    // here, and the top of an octave is just under twice its bottom.
    constexpr auto largest = octave_size(200000, points_for(kind::sized) - 1, points_for(kind::sized));
    auto const g = measure_growth<cand_u64>(largest);
    report_growth("cand", g);
    report_growth("base", measure_growth<base_u64>(largest));
#ifdef UDM_AB_HAVE_BOOST
    if (boost) {
        report_growth("boost", measure_growth<boost_u64>(largest));
    }
#endif
    std::printf("\n");
    std::fflush(stdout);
    // The base size of each workload is the one the scored benchmark uses, and the octave runs from
    // there to just under twice it. What each workload's size means -- and so how many points it is
    // worth measuring at -- is the `kind` argument; see the enum.
#define U64(name, workload, base_n, k) \
    if (want(name))                    \
    compare<workload, base_n, k, base_u64, cand_u64, boost_u64>(name, epochs, boost, g)
#define STR(name, workload, base_n, k) \
    if (want(name))                    \
    compare<workload, base_n, k, base_str, cand_str, boost_str>(name, epochs, boost, g)
#define BIG(name, workload, base_n, k) \
    if (want(name))                    \
    compare<workload, base_n, k, base_big, cand_big, boost_big>(name, epochs, boost, g)
    U64("it64", iterate, 5000, kind::one_size);
    U64("ie64", insert_erase, 20000, kind::grows);
    U64("build64", build, 200000, kind::sized);
    U64("churn64", churn, 50000, kind::sized);
    U64("find64", find_50, 100000, kind::grows);
    STR("itstr", iterate, 5000, kind::one_size);
    STR("iestr", insert_erase, 20000, kind::grows);
    STR("buildstr", build, 200000, kind::sized);
    STR("churnstr", churn, 50000, kind::sized);
    STR("findstr", find_50, 100000, kind::grows);
    BIG("itbig", iterate, 5000, kind::one_size);
    BIG("iebig", insert_erase, 20000, kind::grows);
    BIG("buildbig", build, 200000, kind::sized);
    BIG("churnbig", churn, 50000, kind::sized);
    BIG("findbig", find_50, 100000, kind::grows);
    U64("rhit64", find_hits, 50000, kind::searches);
    U64("rmiss64", find_misses, 50000, kind::searches);
    STR("rhitstr", find_hits, 50000, kind::searches);
    STR("rmissstr", find_misses, 50000, kind::searches);
    STR("hashstr", hash_strings, 0, kind::one_size);
    return 0;
}
