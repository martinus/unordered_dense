// segmented_map's segment size, separated from the page size (#350).
//
//   segment_size <buildfree|find|churn|iterate|rss> <base> [ops]
//
// One variant per binary, picked by three macros, so that no two variants share a code layout or
// an allocator state:
//
//   UDM_SEG_KIND   0: map<uint64_t, size_t> (16 byte pair), 1: map<std::string, size_t> (40),
//                  2: map<uint64_t, big_value> (72)
//   UDM_SEG_BYTES  0 for the plain map (the reference), else MaxSegmentSizeBytes
//   UDM_SEG_HUGE   1 for the huge_page:: aliases, which also put the index on huge pages
//
// The exact-page variant is the same source compiled against segment_exact_page.patch applied to
// the header: MaxSegmentSizeBytes / sizeof(T) elements per segment instead of the power of two
// that fits, each segment one `operator new(4096, align_val_t{4096})`, indexed by a division.
//
// The workloads, the keys and the octave are bench_readme.cpp's (maps.h): build and destroy inside
// the clock, find at half hits, churn at a fixed size, iterate summing `.second`, and peak resident
// memory of a build in a forked child. Every map gets its own default hash.
#include "max_rss.h"

#include "maps.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifndef UDM_SEG_KIND
#    define UDM_SEG_KIND 0
#endif
#ifndef UDM_SEG_BYTES
#    define UDM_SEG_BYTES 0
#endif
#ifndef UDM_SEG_HUGE
#    define UDM_SEG_HUGE 0
#endif

namespace {
using namespace udm_maps;
namespace ud = ankerl::unordered_dense;

#if UDM_SEG_KIND == 1
using one_key = std::string;
using one_val = std::size_t;
#elif UDM_SEG_KIND == 2
using one_key = std::uint64_t;
using one_val = workloads::big_value;
#else
using one_key = std::uint64_t;
using one_val = std::size_t;
#endif
using pair_t = std::pair<one_key, one_val>;

#if UDM_SEG_BYTES == 0 && UDM_SEG_HUGE
using raw_map = ud::huge_page::map<one_key, one_val>;
#elif UDM_SEG_BYTES == 0
using raw_map = ud::map<one_key, one_val>;
#elif UDM_SEG_HUGE
using raw_map = ud::huge_page::segmented_map<one_key,
                                             one_val,
                                             ud::hash<one_key>,
                                             std::equal_to<one_key>,
                                             ud::bucket_type::group,
                                             std::size_t{UDM_SEG_BYTES}>;
#else
using raw_map = ud::segmented_map<one_key,
                                  one_val,
                                  ud::hash<one_key>,
                                  std::equal_to<one_key>,
                                  std::allocator<pair_t>,
                                  ud::bucket_type::group,
                                  std::size_t{UDM_SEG_BYTES}>;
#endif
using map_t = stl_like<raw_map>;

using clock_t_ = std::chrono::steady_clock;

auto octave_size(std::size_t base, std::size_t i) -> std::size_t {
    return static_cast<std::size_t>(static_cast<double>(base) * std::pow(2.0, static_cast<double>(i) / 5.0));
}

// ns per operation at one size, or bytes per entry for `rss`. One discarded round first: the first
// size of an octave otherwise pays the process's first touch of every page (bench_readme.cpp).
auto one_size(std::string const& work, std::size_t n, std::size_t target_ops) -> double {
    auto p = pools<one_key>(n);

    auto once = [&]() -> double {
        if (work == "rss") {
            return max_rss::of([&] {
                       auto m = map_t();
                       fill(m, p);
                       ankerl::nanobench::doNotOptimizeAway(m.size());
                   }) /
                   static_cast<double>(n);
        }
        if (work == "buildfree") {
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
        return once();
    }
    once();
    return once();
}

} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    if (argc < 3) {
        std::printf("usage: %s <buildfree|find|churn|iterate|rss> <base> [ops]\n", argv[0]);
        return 1;
    }
    auto const work = std::string(argv[1]);
    if (work != "buildfree" && work != "find" && work != "churn" && work != "iterate" && work != "rss") {
        std::fprintf(stderr, "unknown workload %s\n", work.c_str());
        return 2;
    }
    auto const base = static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10));
    auto const ops = argc > 3 ? static_cast<std::size_t>(std::strtoull(argv[3], nullptr, 10)) : std::size_t{3'000'000};
    auto acc = 0.0;
    std::printf("%s %zu", work.c_str(), base);
    for (std::size_t i = 0; i < 5; ++i) {
        auto const v = one_size(work, octave_size(base, i), ops);
        acc += std::log(v);
        std::printf(" %.4f", v);
    }
    std::printf(" geomean %.4f\n", std::exp(acc / 5.0));
    return 0;
}
