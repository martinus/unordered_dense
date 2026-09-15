// What the rehash's pipeline is worth at small table sizes.
//
// `fill_buckets_from_values` hashes sixteen elements ahead of the one it places and prefetches the
// group each will land in. That was measured from 200000 entries upwards (notes/index-design.md,
// "The rehash loop pipelined"), where it is 1.26x in cache and up to 2.5x above it. Below 200000 it
// has never been measured, and the loop has no gate -- where `replace()` has one at 256 KB of
// index, because a ring costs about 13 instructions per element whether or not the prefetch it
// issues was needed. A build from empty runs this loop at every doubling, so the small sizes are
// not a corner case.
//
// The subject is `rehash(0)` on a map that is already at the bucket count `rehash(0)` asks for: the
// header then skips the allocation and does exactly `clear_buckets()` plus the loop, so the loop is
// isolated and the allocator is out of it. The memset is in both variants equally and dilutes the
// ratio rather than steering it.
//
// One variant per binary; scripts/ab/rehash_size.sh builds this file twice, once against the
// shipped header and once against a copy with scripts/ab/rehash_plain.patch applied, and alternates
// them round by round.
//
// One *mode* per binary too, which is not a style choice. The two modes lived in one translation
// unit for one afternoon and the second one changed the first: adding `measure_build` and the
// nanobench include to the file moved the pipelined side of the `rehash` mode by 11% -- 1.45 to
// 1.62 ns per element at 22627 entries -- while the plain side did not move at all, and the dip
// this file was written to find went from 4% to 13%. The build is deterministic (same source, same
// flags, same bytes), so that was the inliner and not code layout. REHASH_MODE_BUILD keeps each
// timed function in a translation unit of its own.
//
//   rehash_size <u64|str> <size> [<size>...]     ->  CSV: entries,ns_per_element
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

// Enough repetitions that one measurement is a few milliseconds whatever the size, because the
// clock's resolution and the loop's entry cost are fixed and the work is not. Then the median of
// them, so a stray interrupt lands on one repetition rather than on the answer.
constexpr auto target_ns = std::chrono::nanoseconds{4'000'000}.count();

// Building one from empty, which is what runs the rehash for real: the table doubles as it fills
// and rehashes at every doubling, so this is the loop above weighted the way a caller meets it,
// with the inserts and the growth around it. Warmed first, because a build that starts from a fresh
// arena measures the page fault handler rather than the map.
#if defined(REHASH_MODE_BUILD)

// Somewhere for the result to go that the optimiser cannot reason about, so the build is not
// elided.
volatile std::size_t sink = 0;

template <typename Key>
auto measure_build(std::size_t n) -> double {
    workloads::tame_allocator();

    auto one = [n]() -> std::int64_t {
        auto const t0 = std::chrono::steady_clock::now();
        {
            auto map = ankerl::unordered_dense::map<Key, std::uint64_t>();
            for (auto i = std::uint64_t{}; i < n; ++i) {
                map[workloads::key_source<Key>::get(i)] = i;
            }
            sink = map.size();
        }
        auto const t1 = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    };

    one();
    auto const probe = one();
    auto const reps = static_cast<std::size_t>(
        (std::max)(std::int64_t{3}, (std::min)(std::int64_t{500}, target_ns / (std::max)(probe, std::int64_t{1}))));

    auto samples = std::vector<std::int64_t>();
    samples.reserve(reps);
    for (auto r = std::size_t{}; r < reps; ++r) {
        samples.push_back(one());
    }
    std::sort(samples.begin(), samples.end());
    return static_cast<double>(samples[samples.size() / 2]) / static_cast<double>(n);
}

#else

template <typename Key>
auto measure(std::size_t n) -> double {
    workloads::tame_allocator();

    auto map = ankerl::unordered_dense::map<Key, std::uint64_t>();
    for (auto i = std::uint64_t{}; i < n; ++i) {
        map[workloads::key_source<Key>::get(i)] = i;
    }

    // Settle first: this is the call that may still shrink the bucket array, and it must not be
    // one of the ones that gets timed.
    map.rehash(0);

    auto one = [&map]() -> std::int64_t {
        auto const t0 = std::chrono::steady_clock::now();
        map.rehash(0);
        auto const t1 = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    };

    auto const probe = one();
#    if defined(REHASH_REPS)
    // A repetition count fixed at compile time, so that a `perf stat` run compares two variants
    // that did the same amount of work and can divide by the element count. It is an #if and not a
    // runtime switch on purpose: with the macro undefined the preprocessor leaves this function
    // byte-identical to the one the timing tables were taken with, which a runtime branch would
    // not.
    static_cast<void>(probe);
    auto const reps = std::size_t{REHASH_REPS};
    std::fprintf(stderr, "elements %zu\n", reps * n);
#    else
    auto const reps = static_cast<std::size_t>(
        (std::max)(std::int64_t{3}, (std::min)(std::int64_t{2000}, target_ns / (std::max)(probe, std::int64_t{1}))));
#    endif
    // A fixed count so that `perf stat` over the whole process compares two variants that did the
    // same amount of work; the ratio of instructions is the whole point there and the automatic
    // count differs between them.

    auto samples = std::vector<std::int64_t>();
    samples.reserve(reps);
    for (auto r = std::size_t{}; r < reps; ++r) {
        samples.push_back(one());
    }
    std::sort(samples.begin(), samples.end());

    // The map must survive the clock, or nothing above is bound to it.
    if (map.size() != n) {
        std::fprintf(stderr, "map lost elements: %zu of %zu\n", map.size(), n);
        std::exit(1);
    }
    auto const median = static_cast<double>(samples[samples.size() / 2]);
    return median / static_cast<double>(n);
}

#endif

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <u64|str> <rehash|build> <size> [<size>...]\n", argv[0]);
        return 1;
    }
    auto const key = std::string(argv[1]);
    auto const mode = std::string(argv[2]);
    if (key != "u64" && key != "str") {
        std::fprintf(stderr, "key must be u64 or str, not %s\n", key.c_str());
        return 1;
    }
#if defined(REHASH_MODE_BUILD)
    auto const built_for = std::string("build");
#else
    auto const built_for = std::string("rehash");
#endif
    if (mode != built_for) {
        std::fprintf(stderr, "this binary was built for %s, not %s\n", built_for.c_str(), mode.c_str());
        return 1;
    }
    for (auto i = 3; i < argc; ++i) {
        auto const n = static_cast<std::size_t>(std::strtoull(argv[i], nullptr, 10));
#if defined(REHASH_MODE_BUILD)
        auto const ns = key == "u64" ? measure_build<std::uint64_t>(n) : measure_build<std::string>(n);
#else
        auto const ns = key == "u64" ? measure<std::uint64_t>(n) : measure<std::string>(n);
#endif
        std::printf("%zu,%.4f\n", n, ns);
        std::fflush(stdout);
    }
    return 0;
}
