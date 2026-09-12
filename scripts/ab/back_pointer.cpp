// What a slot back-pointer per value is worth, across the size axis and per workload (#266).
//
// `finish_erase` closes the hole an erase leaves by moving the last value into it, and to repoint
// that value's slot it hashes the moved key a second time and walks to it. For an integer key that
// is free; for a string key behind a heap pointer it is about 50 ns. The back-pointer removes it,
// and charges four bytes per entry and a store on every insert for the privilege.
//
//   back_pointer <base|bp> <u64|str|big> <build|churn|erasekey|eraseiter|find|mem> <n>
//
// prints ns per operation, or live bytes per entry for `mem`. The variant is chosen at build time
// by -DBP_ONE_SIDE: one variant per binary, because two headers in one translation unit share an
// inlining budget and this change alters `finish_erase`'s size -- see scripts/ab/window.cpp. The
// second header is the shipped one with back_pointer.patch applied, renamed into its own namespace
// by back_pointer.sh.
//
// The sizes are runtime values here, which the score's workloads do not allow themselves. Measured
// on 2026-09-12 (notes/index-design.md, "The sizes are template arguments"), that is worth at most
// 1.4%, inside the layout band, and both variants pay it identically.
#include "bp.h"

#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>
#include <third-party/nanobench.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

#if !defined(BP_ONE_SIDE)
#    error "build with -DBP_ONE_SIDE=0 for the shipped header or =1 for the patched one; see back_pointer.sh"
#endif

namespace {

// Live bytes, not the sum of everything a build ever asked for: a size header on each block so the
// delete can subtract, which is scripts/ab/memory.cpp's counting pair and is copied from it. It has
// to be a global operator and not an allocator the map is given, because the back-pointer's own
// vector is on the default allocator and an allocator-side counter cannot see it.
std::size_t g_live = 0;

} // namespace

auto operator new(std::size_t n) -> void* {
    auto* p = std::malloc(n + 16); // NOLINT(cppcoreguidelines-no-malloc,hicpp-no-malloc)
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    *static_cast<std::size_t*>(p) = n;
    g_live += n;
    return static_cast<char*>(p) + 16;
}

void operator delete(void* p) noexcept {
    if (p == nullptr) {
        return;
    }
    auto* base = static_cast<char*>(p) - 16;
    g_live -= *reinterpret_cast<std::size_t*>(base); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    std::free(base);                                 // NOLINT(cppcoreguidelines-no-malloc,hicpp-no-malloc)
}

void operator delete(void* p, std::size_t /*n*/) noexcept {
    ::operator delete(p);
}

namespace {

// The keys, built once per process and before anything is timed or counted: making a string key is
// a memcpy into a shared buffer, and a workload that makes its keys inside the timed region
// measures that instead.
template <typename Key>
auto make_keys(std::size_t n) -> std::vector<Key> {
    auto keys = std::vector<Key>();
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        keys.push_back(workloads::key_source<Key>::get(i * UINT64_C(0x9E3779B97F4A7C15) + 12345));
    }
    return keys;
}

// A shuffled order to touch them in, so no workload here walks the table in insertion order.
auto make_order(std::size_t n) -> std::vector<std::uint32_t> {
    auto order = std::vector<std::uint32_t>(n);
    for (std::size_t i = 0; i < n; ++i) {
        order[i] = static_cast<std::uint32_t>(i);
    }
    ankerl::nanobench::Rng(4711).shuffle(order);
    return order;
}

template <typename Map>
void fill(Map* map, std::vector<typename Map::key_type> const& keys) {
    for (std::size_t i = 0; i < keys.size(); ++i) {
        (*map)[keys[i]] = i;
    }
}

template <typename Map>
auto measure(std::string const& work, std::size_t n) -> double {
    using clock = std::chrono::steady_clock;
    using key_type = typename Map::key_type;
    workloads::tame_allocator();
    auto const keys = make_keys<key_type>(n);
    auto const order = make_order(n);

    // ops is what the cell is divided by; body is what is timed and nothing else.
    auto timed = [](std::size_t ops, auto&& body) {
        auto const t0 = clock::now();
        body();
        auto const t1 = clock::now();
        return static_cast<double>((t1 - t0).count()) / static_cast<double>(ops);
    };

    if (work == "mem") {
        auto map = Map();
        auto const before = g_live;
        fill(&map, keys);
        return static_cast<double>(g_live - before) / static_cast<double>(n);
    }

    if (work == "build") {
        auto map = Map();
        return timed(n, [&] {
            fill(&map, keys);
            ankerl::nanobench::doNotOptimizeAway(map.size());
        });
    }

    // Everything below measures a table that is already there, so the fill is outside the timed
    // region and what is timed is the operation the cell is named after.
    auto map = Map();
    fill(&map, keys);

    if (work == "find") {
        return timed(n, [&] {
            std::size_t checksum = 0;
            for (auto i : order) {
                auto it = map.find(keys[i]);
                if (it != map.end()) {
                    checksum += it->second;
                }
            }
            ankerl::nanobench::doNotOptimizeAway(checksum);
        });
    }

    if (work == "erasekey") {
        return timed(n, [&] {
            for (auto i : order) {
                map.erase(keys[i]);
            }
            ankerl::nanobench::doNotOptimizeAway(map.size());
        });
    }

    if (work == "eraseiter") {
        // find then erase(iterator), which is the pattern the back-pointer's other half exists for:
        // the slot comes from the array instead of a second walk, but the counters still want the
        // hash, so this is not the no-hash erase the distance nibbles would give.
        return timed(n, [&] {
            for (auto i : order) {
                auto it = map.find(keys[i]);
                if (it != map.end()) {
                    map.erase(it);
                }
            }
            ankerl::nanobench::doNotOptimizeAway(map.size());
        });
    }

    if (work == "churn") {
        // The score's churn at a size the score never reaches, and in its shape: erase one live key,
        // insert one the table has never held, hold the size exactly. `live` holds key *values* and
        // the key is made from one, the way workloads::churn does it -- an earlier version indexed a
        // vector of 3n prepared keys instead, which put two random reads into a 384 MB array inside
        // the timed region at four million entries and diluted the ratio toward 1.00 with work
        // neither variant does.
        auto const rounds = std::size_t{2};
        auto live = std::vector<std::uint64_t>(n);
        for (std::size_t i = 0; i < n; ++i) {
            live[i] = i * UINT64_C(0x9E3779B97F4A7C15) + 12345;
        }
        auto next = n;
        auto rng = ankerl::nanobench::Rng(31337);
        return timed(rounds * n * 2, [&] {
            for (std::size_t r = 0; r < rounds * n; ++r) {
                auto const slot = static_cast<std::size_t>(((rng() >> 32U) * live.size()) >> 32U);
                map.erase(workloads::key_source<key_type>::get(live[slot]));
                auto const fresh = next * UINT64_C(0x9E3779B97F4A7C15) + 12345;
                map[workloads::key_source<key_type>::get(fresh)] = r;
                live[slot] = fresh;
                ++next;
            }
            ankerl::nanobench::doNotOptimizeAway(map.size());
        });
    }

    std::fprintf(stderr, "unknown workload %s\n", work.c_str());
    std::exit(2);
}

// The one variant this binary holds. `if constexpr` rather than a ternary in main: a runtime choice
// would instantiate both maps in both binaries, which is the thing one-variant-per-binary exists to
// prevent.
template <typename K, typename V>
using map_t = std::conditional_t<BP_ONE_SIDE != 0, udmbp::unordered_dense::map<K, V>, ankerl::unordered_dense::map<K, V>>;

auto measure_keys(std::string const& keys, std::string const& work, std::size_t n) -> double {
    if (keys == "u64") {
        return measure<map_t<std::uint64_t, std::size_t>>(work, n);
    }
    if (keys == "str") {
        return measure<map_t<std::string, std::size_t>>(work, n);
    }
    if (keys == "big") {
        // The 64 byte mapped value: the four bytes per entry are a much smaller share of it, which
        // is the shape most favourable to the back-pointer's memory cost.
        return measure<map_t<std::uint64_t, workloads::big_value>>(work, n);
    }
    std::fprintf(stderr, "keys must be u64, str or big\n");
    std::exit(2);
}

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 5) {
        std::printf("usage: %s <base|bp> <u64|str|big> <build|churn|erasekey|eraseiter|find|mem> <n>\n", argv[0]);
        return 1;
    }
    auto const variant = std::string(argv[1]);
    if ((variant == "bp") != (BP_ONE_SIDE != 0)) {
        std::fprintf(stderr, "this binary holds only the %s side\n", BP_ONE_SIDE != 0 ? "bp" : "base");
        return 2;
    }
    std::printf("%.3f\n", measure_keys(std::string(argv[2]), std::string(argv[3]), std::strtoull(argv[4], nullptr, 10)));
    return 0;
}
