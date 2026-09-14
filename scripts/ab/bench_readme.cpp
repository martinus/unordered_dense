// The README's two benchmark graphs: every map a caller might reach for, in the configuration a
// caller gets by typing its type name, on the five things the README claims something about.
//
//   bench_readme <build|find|churn|iterate|memory> <base> [rounds]
//
// One map per binary -- `-DUDM_ONE_MAP=<index into maps_for<Key>::type>`, `-DUDM_ONE_STR` for the
// string key -- because a binary holding a dozen maps has a code layout that moves by more than the
// differences being drawn, and because a map that shares a process with eleven rivals shares their
// allocator state too. bench_readme.sh builds the set and collects the numbers.
//
// Every map is handed its *own* hash here (maps.h's UDM_DEFAULT_HASH), which is the one thing this
// harness does differently from maps.cpp next door. maps.cpp asks which index is faster and gives
// them all the same hash so that the index is what differs; a README graph is about what a caller
// gets, and what a caller gets includes the hash. For a string key the two questions have different
// answers, and both are worth having.
//
// Sizes are an octave: five points from n to just under 2n, geometric mean. A table's load factor
// sweeps a sawtooth between doublings and two maps double at different sizes, so a ratio taken at
// one size is a ratio between two arbitrary points of two different cycles -- measured at up to 26%
// for a cross-family pair (notes/index-design.md, "Fifty points draw the load-factor sawtooth").
#include "count_alloc.h"
#include "max_rss.h"

#include "maps.h"

#include <dlfcn.h>
#include <sys/mman.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#ifndef UDM_ONE_MAP
#    define UDM_ONE_MAP 0
#endif

// The allocation counter is scripts/ab/count_alloc.h, shared with maps.cpp.

namespace {
using namespace udm_maps;

#if defined(UDM_ONE_STR)
using one_key = std::string;
#else
using one_key = std::uint64_t;
#endif
using one_val = std::size_t;
using map_t = std::tuple_element_t<UDM_ONE_MAP, typename maps_for<one_key, one_val>::type>;

using clock_t_ = std::chrono::steady_clock;

// One octave, log-spaced: the i-th of five points is base * 2^(i/5), so the set spans exactly one
// doubling whatever the base is.
auto octave_size(std::size_t base, std::size_t i) -> std::size_t {
    return static_cast<std::size_t>(static_cast<double>(base) * std::pow(2.0, static_cast<double>(i) / 5.0));
}

// How much work a timed region does, whatever the table size: a cell that runs for a millisecond
// measures the machine's mood, and the ratios this chart draws are 3-20% apart. Ten million
// operations is 1.4 to 6.2 seconds per cell depending on the map and the workload, which is where
// repeated runs of the same cell agree to under a percent.
constexpr std::size_t target_ops = 10'000'000;

// ns per operation at one size, or bytes per entry for `memory`.
//
// One untimed round first, then the timed one. Without the warm-up the first size of an octave
// reads up to 2.3x the third, because it is the one paying for the process's first touch of every
// page it asks for. There is no round count here on purpose: the repeats that get medianed are the
// shell's, which interleaves them across every map rather than repeating one cell back to back, so
// that a machine drifting across the run drifts across all of them.
auto one_size(std::string const& work, std::size_t n) -> double {
    auto p = pools<one_key>(n);

    auto once = [&]() -> double {
        if (work == "memory") {
            if (!count_alloc::available()) {
                std::fprintf(stderr, "this binary was built without UDM_COUNT_ALLOC and cannot count bytes\n");
                std::exit(2);
            }
            // The keys are built before the counter goes true, so the pools are not charged to the
            // map. What is reported is the peak of the build, which includes the doubling: the
            // moment the old array and the new one are both alive is where a caller's ceiling is.
            return count_alloc::of([&] {
                       auto m = map_t();
                       fill(m, p);
                   }).peak /
                   static_cast<double>(n);
        }
        if (work == "rss") {
            return max_rss::of([&] {
                       auto m = map_t();
                       fill(m, p);
                       ankerl::nanobench::doNotOptimizeAway(m.size());
                   }) /
                   static_cast<double>(n);
        }
        if (work == "buildfree") {
            // Build *and* tear down: the same loop as `build` with the destructor left inside the
            // clock, so that the two panels together say how much of a map's lifetime cost is the
            // giving back. A node map hands a million blocks to free(); a dense map frees two.
            auto const builds = target_ops / n + 1;
            auto acc = std::size_t{0};
            auto const t0 = clock_t_::now();
            for (std::size_t i = 0; i < builds; ++i) {
                auto m = map_t();
                fill(m, p);
                acc += m.size();
            }
            auto const t1 = clock_t_::now();
            ankerl::nanobench::doNotOptimizeAway(acc);
            return static_cast<double>((t1 - t0).count()) / static_cast<double>(builds * n);
        }
        if (work == "build") {
            // The only workload whose subject is created inside the timed region, because creating
            // it is what it measures. Its *destruction* is outside, which maps.h's build() cannot
            // do because it returns after the map has gone: giving back a million nodes costs
            // `std::unordered_map` about a third of this loop again and a dense map nothing, so
            // timing it would put a teardown difference on a panel named for inserts.
            auto const builds = target_ops / n + 1;
            auto acc = std::size_t{0};
            auto total = clock_t_::duration::zero();
            for (std::size_t i = 0; i < builds; ++i) {
                auto const t0 = clock_t_::now();
                auto m = map_t();
                fill(m, p);
                auto const t1 = clock_t_::now();
                total += t1 - t0;
                acc += m.size();
            }
            ankerl::nanobench::doNotOptimizeAway(acc);
            return static_cast<double>(total.count()) / static_cast<double>(builds * n);
        }

        // Everything else measures a table that is already there, so the fill is outside the timed
        // region: what is timed is the operation the panel is named after.
        auto m = map_t();
        fill(m, p);
        auto ops = target_ops;
        auto const t0 = clock_t_::now();
        if (work == "find") {
            auto st = lookup_state();
            ankerl::nanobench::doNotOptimizeAway(lookups(m, p, st, asking::half, ops));
        } else if (work == "churn") {
            auto rng = ankerl::nanobench::Rng(7);
            auto tick = std::size_t{0};
            churn(m, p.present, p.spare, rng, ops, tick);
            ankerl::nanobench::doNotOptimizeAway(m.size());
        } else {
            // One operation is one element visited, so the round count is what makes the element
            // count the same for every size.
            auto const sweeps = target_ops / n + 1;
            auto acc = std::size_t{0};
            for (std::size_t i = 0; i < sweeps; ++i) {
                acc += m.sum();
            }
            ankerl::nanobench::doNotOptimizeAway(acc);
            ops = sweeps * n;
        }
        auto const t1 = clock_t_::now();
        return static_cast<double>((t1 - t0).count()) / static_cast<double>(ops);
    };

    if (work == "rss") {
        return once(); // each call forks; there is no process state to warm
    }
    once(); // discarded
    return once();
}

} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    if (argc < 3) {
        std::printf("usage: %s <build|buildfree|find|churn|iterate|memory|rss> <base>\n", argv[0]);
        return 1;
    }
    auto const work = std::string(argv[1]);
    auto const base = static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10));
    // One list, checked before a million-entry table gets built rather than after.
    static constexpr auto known =
        std::array<char const*, 7>{"build", "buildfree", "find", "churn", "iterate", "memory", "rss"};
    if (std::none_of(known.begin(), known.end(), [&](char const* k) {
            return work == k;
        })) {
        std::fprintf(stderr, "unknown workload %s\n", work.c_str());
        return 2;
    }

    // The geometric mean over the octave, which is the summary a ratio between two maps can be
    // taken from; the per-size numbers are printed beside it so a wild one is visible.
    auto acc = 0.0;
    std::printf("%s %s", map_t::name, work.c_str());
    for (std::size_t i = 0; i < 5; ++i) {
        auto const v = one_size(work, octave_size(base, i));
        acc += std::log(v);
        std::printf(" %.4f", v);
    }
    std::printf(" geomean %.4f\n", std::exp(acc / 5.0));
    return 0;
}
