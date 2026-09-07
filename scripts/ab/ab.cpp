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
#include "base.h"

#include <ankerl/unordered_dense.h>
#include <bench/workloads.h>
#include <third-party/nanobench.h>
#ifdef UDM_AB_HAVE_BOOST
#    include <boost/unordered/unordered_flat_map.hpp>
#endif

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

// Five points from n to just under 2n, evenly spaced in log2 so the octave is sampled uniformly:
// 2^(k/5), as ten-thousandths. Five is enough to average out the sawtooth and few enough that a
// run stays minutes rather than an hour; the shape within an octave is the size sweep's job
// (scripts/ab/sweep.cpp), not the score's.
constexpr size_t max_points = 5;
constexpr size_t octave_scale[max_points] = {10000, 11487, 13195, 15157, 17411};

constexpr auto octave_size(size_t n, size_t i) -> size_t {
    return n * octave_scale[i] / 10000;
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
    // Output suppressed: five points times twenty workloads is a hundred nanobench tables, and the
    // per-point line printed below carries what each of them would have said.
    auto bench = ankerl::nanobench::Bench().title(name).epochs(epochs).performanceCounters(false).output(nullptr);
    auto base = [] {
        ankerl::nanobench::doNotOptimizeAway(scalar(Workload<Base, N>::run()));
    };
    auto cand = [] {
        ankerl::nanobench::doNotOptimizeAway(scalar(Workload<Cand, N>::run()));
    };
    auto boost = [] {
        ankerl::nanobench::doNotOptimizeAway(scalar(Workload<Boost, N>::run()));
    };
    auto const res = with_boost ? bench.compare("base", base, "cand", cand, "boost", boost)
                                : bench.compare("base", base, "cand", cand);
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

void report(char const* name, std::vector<point> const& pts, bool with_boost) {
    std::printf("%s\n", name);
    auto cand_ratios = std::vector<double>();
    auto boost_ratios = std::vector<double>();
    for (auto const& p : pts) {
        cand_ratios.push_back(p.cand_ratio);
        boost_ratios.push_back(p.boost_ratio);
        // hashstr has no table, so it has no size to report -- it is given a base size of zero.
        if (p.n == 0) {
            std::printf("  %-11s base ", "no table");
        } else {
            std::printf("  n=%-9zu base ", p.n);
        }
        print_time(p.base_ns);
        std::printf("  cand ");
        print_time(p.cand_ns);
        std::printf("   %6.3f [%.3f .. %.3f]", p.cand_ratio, p.cand_lo, p.cand_hi);
        if (with_boost) {
            std::printf("   boost/cand %6.3f", p.boost_ratio);
        }
        std::printf("\n");
    }
    auto const g = geomean(cand_ratios);
    auto const gb = geomean(boost_ratios);
    if (pts.size() > 1) {
        std::printf("  octave geomean %.4f", g);
        if (with_boost) {
            std::printf("   boost/cand %.4f", gb);
        }
        std::printf("   (%zu points, %zu .. %zu)\n", pts.size(), pts.front().n, pts.back().n);
    }
    // The machine-readable line summarize.py reads. One per workload, whatever the point count, so
    // a swept run and a single-size run are summarised by the same code.
    std::printf("#ratio %s %.6f %.6f %zu\n\n", name, g, gb, pts.size());
    std::fflush(stdout);
}

template <template <typename, size_t> class Workload,
          size_t BaseN,
          typename Base,
          typename Cand,
          typename Boost,
          size_t... I>
void compare_seq(char const* name, size_t epochs, size_t points, bool with_boost, std::index_sequence<I...> /*unused*/) {
    auto pts = std::vector<point>();
    // Every point is instantiated, only the wanted ones run: the sizes are template arguments, so
    // that a workload's default instantiation stays the exact code the scored benchmark compiles.
    (void)std::initializer_list<int>{
        (I < points ? (pts.push_back(measure_at<Workload, Base, Cand, Boost, octave_size(BaseN, I)>(name, epochs, with_boost)), 0)
                    : 0)...};
    report(name, pts, with_boost);
}

template <template <typename, size_t> class Workload, size_t BaseN, typename Base, typename Cand, typename Boost>
void compare(char const* name, size_t epochs, size_t points, bool with_boost) {
    compare_seq<Workload, BaseN, Base, Cand, Boost>(
        name, epochs, points > max_points ? max_points : points, with_boost, std::make_index_sequence<max_points>{});
}

// The workloads as templates with a map and a size, so that compare() can name them and sweep them.
// Each is instantiated once per octave point; the default size never appears here, it belongs to
// the scored benchmark.
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
template <typename Map, size_t N>
struct find_hits {
    static auto run() {
        return workloads::find_all<Map, true, N>();
    }
};
template <typename Map, size_t N>
struct find_misses {
    static auto run() {
        return workloads::find_all<Map, false, N>();
    }
};

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 2) {
        std::printf("usage: %s <workload|all> [epochs=12] [boost=0] [points=5]\n"
                    "workloads: it64 ie64 build64 churn64 find64 itstr iestr buildstr churnstr findstr\n"
                    "           itbig iebig buildbig churnbig findbig (64 byte mapped value)\n"
                    "           (bench_quick_overall_udm)\n"
                    "           rhit64 rmiss64 rhitstr rmissstr (all hits / no hits)\n"
                    "           hashstr (the string hash on its own)\n"
                    "\npoints is how many sizes across one octave each workload is measured at, and\n"
                    "what is reported is the geometric mean of those ratios. 1 restores the single\n"
                    "size the score used before 2026-09-07, which is only comparable with itself.\n",
                    argv[0]);
        return 1;
    }
    std::string w = argv[1];
    auto epochs = static_cast<size_t>(argc > 2 ? std::atoi(argv[2]) : 12);
    bool boost = argc > 3 && std::atoi(argv[3]) != 0;
    auto points = static_cast<size_t>(argc > 4 ? std::atoi(argv[4]) : static_cast<int>(max_points));
    if (points < 1) {
        points = 1;
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
    // The base size of each workload is the one the scored benchmark uses, and the octave runs from
    // there to just under twice it.
    //
    // Iteration and the hash workload are measured at one point on purpose. Iteration walks the
    // dense value vector, which has no buckets and so no sawtooth to average out, and its cost is
    // quadratic in the element count -- sweeping an octave would make it three times the run for an
    // answer that does not change. hashstr never touches a table at all.
#define U64(name, workload, base_n, pts) \
    if (want(name))                      \
    compare<workload, base_n, base_u64, cand_u64, boost_u64>(name, epochs, (pts), boost)
#define STR(name, workload, base_n, pts) \
    if (want(name))                      \
    compare<workload, base_n, base_str, cand_str, boost_str>(name, epochs, (pts), boost)
#define BIG(name, workload, base_n, pts) \
    if (want(name))                      \
    compare<workload, base_n, base_big, cand_big, boost_big>(name, epochs, (pts), boost)
    U64("it64", iterate, 5000, 1);
    U64("ie64", insert_erase, 20000, points);
    U64("build64", build, 200000, points);
    U64("churn64", churn, 50000, points);
    U64("find64", find_50, 100000, points);
    STR("itstr", iterate, 5000, 1);
    STR("iestr", insert_erase, 20000, points);
    STR("buildstr", build, 200000, points);
    STR("churnstr", churn, 50000, points);
    STR("findstr", find_50, 100000, points);
    BIG("itbig", iterate, 5000, 1);
    BIG("iebig", insert_erase, 20000, points);
    BIG("buildbig", build, 200000, points);
    BIG("churnbig", churn, 50000, points);
    BIG("findbig", find_50, 100000, points);
    U64("rhit64", find_hits, 50000, points);
    U64("rmiss64", find_misses, 50000, points);
    STR("rhitstr", find_hits, 50000, points);
    STR("rmissstr", find_misses, 50000, points);
    STR("hashstr", hash_strings, 0, 1);
    return 0;
}
