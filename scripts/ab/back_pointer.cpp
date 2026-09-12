// What a slot back-pointer per value is worth, across the size axis and per workload (#266).
//
// `finish_erase` closes the hole an erase leaves by moving the last value into it, and to repoint
// that value's slot it hashes the moved key a second time and walks to it. For an integer key that
// is free; for a string key behind a heap pointer it is about 50 ns. The back-pointer removes it,
// and charges four bytes per entry and a store on every insert for the privilege.
//
//   back_pointer <base|bp> <u64|str> <build|churn|erasekey|eraseiter|find|mem> <n> [rounds]
//
// prints ns per operation, or bytes per entry for `mem`. The two variants are two instantiations of
// the same header, the second compiled with ANKERL_UNORDERED_DENSE_SLOT_BACK_POINTER=1 into its own
// namespace by back_pointer.sh -- so a cell is one template instantiation and never a branch, and
// one process measures one cell.
//
// The sizes are runtime values here, which the score's workloads do not allow themselves. Measured
// on 2026-09-12 (notes/index-design.md, "The sizes are template arguments"), that is worth at most
// 1.4%, inside the layout band, and both variants pay it identically.
#include "bp.h"

#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>
#include <third-party/nanobench.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

// Every allocation the process makes, counted through a replaced global operator new, because the
// back-pointer array is on the default allocator and an allocator-side counter cannot see it. The
// map's own allocations go through the same operator, so one counter covers both variants.
std::size_t g_allocated = 0;
bool g_counting = false;

} // namespace

auto operator new(std::size_t n) -> void* {
    if (g_counting) {
        g_allocated += n;
    }
    auto* p = std::malloc(n); // NOLINT(cppcoreguidelines-no-malloc,hicpp-no-malloc)
    if (p == nullptr) {
        std::abort();
    }
    return p;
}

void operator delete(void* p) noexcept {
    std::free(p); // NOLINT(cppcoreguidelines-no-malloc,hicpp-no-malloc)
}

void operator delete(void* p, std::size_t /*unused*/) noexcept {
    std::free(p); // NOLINT(cppcoreguidelines-no-malloc,hicpp-no-malloc)
}

namespace {

// The keys, built once and shared by every round: making a string key is a memcpy into a shared
// buffer, and a workload that makes its keys inside the timed region measures that instead.
template <typename Key>
auto keys_for(std::size_t n) -> std::vector<Key> const& {
    static auto cache = std::vector<Key>();
    if (cache.size() < n) {
        cache.clear();
        cache.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            cache.push_back(workloads::key_source<Key>::get(i * UINT64_C(0x9E3779B97F4A7C15) + 12345));
        }
    }
    return cache;
}

// A shuffled order to touch them in, so no workload here walks the table in insertion order.
auto shuffled(std::size_t n) -> std::vector<std::uint32_t> const& {
    static auto cache = std::vector<std::uint32_t>();
    if (cache.size() != n) {
        cache.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            cache[i] = static_cast<std::uint32_t>(i);
        }
        auto rng = ankerl::nanobench::Rng(4711);
        rng.shuffle(cache);
    }
    return cache;
}

struct result {
    double ns_per_op = 0;
    double bytes_per_entry = 0;
};

template <typename Map>
auto fill(Map* map, std::size_t n) {
    auto const& keys = keys_for<typename Map::key_type>(n);
    for (std::size_t i = 0; i < n; ++i) {
        (*map)[keys[i]] = i;
    }
}

template <typename Map>
auto measure(std::string const& work, std::size_t n) -> result {
    using clock = std::chrono::steady_clock;
    workloads::tame_allocator();
    auto const& keys = keys_for<typename Map::key_type>(n);
    auto const& order = shuffled(n);

    if (work == "mem") {
        // Live bytes with n entries in the map: every allocation minus every free, so what is
        // reported is the steady footprint and not the sum of everything a build ever asked for.
        auto map = Map();
        g_allocated = 0;
        g_counting = true;
        fill(&map, n);
        auto const bytes = g_allocated;
        g_counting = false;
        return {0.0, static_cast<double>(bytes) / static_cast<double>(n)};
    }

    if (work == "build") {
        auto map = Map();
        auto const t0 = clock::now();
        fill(&map, n);
        auto const t1 = clock::now();
        ankerl::nanobench::doNotOptimizeAway(map.size());
        return {static_cast<double>((t1 - t0).count()) / static_cast<double>(n), 0.0};
    }

    // Everything below measures a table that is already there, so the fill is outside the timed
    // region and what is timed is the operation the cell is named after.
    auto map = Map();
    fill(&map, n);

    if (work == "find") {
        auto const t0 = clock::now();
        std::size_t checksum = 0;
        for (auto i : order) {
            auto it = map.find(keys[i]);
            if (it != map.end()) {
                checksum += it->second;
            }
        }
        auto const t1 = clock::now();
        ankerl::nanobench::doNotOptimizeAway(checksum);
        return {static_cast<double>((t1 - t0).count()) / static_cast<double>(n), 0.0};
    }

    if (work == "erasekey") {
        auto const t0 = clock::now();
        for (auto i : order) {
            map.erase(keys[i]);
        }
        auto const t1 = clock::now();
        ankerl::nanobench::doNotOptimizeAway(map.size());
        return {static_cast<double>((t1 - t0).count()) / static_cast<double>(n), 0.0};
    }

    if (work == "eraseiter") {
        // find then erase(iterator), which is the pattern the back-pointer's other half exists for:
        // the slot comes from the array instead of a second walk, but the counters still want the
        // hash, so this is not the no-hash erase the distance nibbles would give.
        auto const t0 = clock::now();
        for (auto i : order) {
            auto it = map.find(keys[i]);
            if (it != map.end()) {
                map.erase(it);
            }
        }
        auto const t1 = clock::now();
        ankerl::nanobench::doNotOptimizeAway(map.size());
        return {static_cast<double>((t1 - t0).count()) / static_cast<double>(n), 0.0};
    }

    if (work == "churn") {
        // The score's churn shape at a size the score never reaches: erase one live key, insert one
        // the table has never held, and hold the size exactly. Every erase moves the back element
        // into the hole, which is the operation under test.
        auto const rounds = std::size_t{2};
        auto live = std::vector<std::uint32_t>(order.begin(), order.end());
        auto const& fresh = keys_for<typename Map::key_type>(n * (rounds + 1));
        auto next = n;
        auto rng = ankerl::nanobench::Rng(31337);
        auto const t0 = clock::now();
        for (std::size_t r = 0; r < rounds * n; ++r) {
            auto const slot = static_cast<std::size_t>(((rng() >> 32U) * live.size()) >> 32U);
            map.erase(fresh[live[slot]]);
            map[fresh[next]] = r;
            live[slot] = static_cast<std::uint32_t>(next);
            ++next;
        }
        auto const t1 = clock::now();
        ankerl::nanobench::doNotOptimizeAway(map.size());
        return {static_cast<double>((t1 - t0).count()) / static_cast<double>(rounds * n * 2), 0.0};
    }

    std::fprintf(stderr, "unknown workload %s\n", work.c_str());
    std::exit(2);
}

template <template <typename, typename> class Map>
auto measure_keys(std::string const& keys, std::string const& work, std::size_t n) -> result {
    if (keys == "u64") {
        return measure<Map<std::uint64_t, std::size_t>>(work, n);
    }
    if (keys == "str") {
        return measure<Map<std::string, std::size_t>>(work, n);
    }
    std::fprintf(stderr, "keys must be u64 or str\n");
    std::exit(2);
}

template <typename K, typename V>
using base_map = ankerl::unordered_dense::map<K, V>;
template <typename K, typename V>
using bp_map = udmbp::unordered_dense::map<K, V>;

// One variant per binary, which is the control for the two-in-one-binary default: two headers in
// one translation unit share an inlining budget, and this file's own rules say a change that alters
// what gets inlined reads the wrong sign there. -DBP_ONE_SIDE=0 builds the baseline alone and =1 the
// back-pointer alone, so the pair can be alternated the way scripts/ab/solo.sh alternates binaries.
#if defined(BP_ONE_SIDE)
constexpr bool one_side_is_bp = BP_ONE_SIDE != 0;
#endif

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 5) {
        std::printf("usage: %s <base|bp> <u64|str> <build|churn|erasekey|eraseiter|find|mem> <n>\n", argv[0]);
        return 1;
    }
    auto const variant = std::string(argv[1]);
    auto const keys = std::string(argv[2]);
    auto const work = std::string(argv[3]);
    auto const n = static_cast<std::size_t>(std::strtoull(argv[4], nullptr, 10));

#if defined(BP_ONE_SIDE)
    if ((variant == "bp") != one_side_is_bp) {
        std::fprintf(stderr, "this binary holds only the %s side\n", one_side_is_bp ? "bp" : "base");
        return 2;
    }
    auto const r = one_side_is_bp ? measure_keys<bp_map>(keys, work, n) : measure_keys<base_map>(keys, work, n);
#else
    auto const r = variant == "bp" ? measure_keys<bp_map>(keys, work, n) : measure_keys<base_map>(keys, work, n);
#endif
    if (work == "mem") {
        std::printf("%.3f\n", r.bytes_per_entry);
    } else {
        std::printf("%.3f\n", r.ns_per_op);
    }
    return 0;
}
