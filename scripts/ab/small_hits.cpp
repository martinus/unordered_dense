// Hits in cache-resident tables, one header per binary (#346): bench_small_hits_udm's two loops,
// workloads::find_all<true> (quiet) and workloads::find_hits_busy (busy, shaped like #317's caller),
// for map<uint64_t, size_t> and map<std::string, size_t>, at 1000, 4000 and 16000 entries. Each size
// is five tables across its octave (n * 2^(k/5)), because load factor sawtooths between doublings;
// per table the median of 5 timed runs of a million lookups after one warm-up run; per size the
// geometric mean of the five. workloads.h uses only the map's standard API, so this builds against
// any version of the header, 4.x included, which the test suite does not.
//
// A third key type, "seq", is map<uint64_t, size_t> filled with 0..n-1 as they are, not through
// workloads.h's bijection: ids that are dense indices, as Redpanda's raft groups and OSRM's nodes
// are. A multiplicative hash spreads those on a lattice without collisions, which is 4.x's best
// case; the score's rule scrambles integer keys precisely to keep it out of the score.
//
//   clang++ -O3 -DNDEBUG -std=c++17 -I<header dir> -Itest scripts/ab/small_hits.cpp
//   ./a.out      # prints: key n mode ns/lookup
#define ANKERL_NANOBENCH_IMPLEMENT
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>
#ifdef WITH_ID_MAP
#    include "id_map/id_map.h" // #379: -DWITH_ID_MAP adds "seq_id_map", the dense ids in the id_map prototype
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

namespace {

volatile std::size_t sink = 0;

template <typename F>
auto median_ns(F&& f) -> double {
    sink = sink + f(); // warm-up
    auto times = std::vector<double>{};
    for (int i = 0; i < 5; ++i) {
        auto const t0 = std::chrono::steady_clock::now();
        sink = sink + f();
        auto const t1 = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / 1e6);
    }
    std::sort(times.begin(), times.end());
    return times[2];
}

template <typename T>
struct is_dense_ids : std::false_type {};
template <typename Map>
struct is_dense_ids<workloads::dense_id_table<Map>> : std::true_type {};

template <typename Map>
void sizes(char const* key, workloads::busy_sink* out) {
    for (std::size_t n : {std::size_t{1000}, std::size_t{4000}, std::size_t{16000}}) {
        double quiet = 0;
        double busy = 0;
        for (int k = 0; k < 5; ++k) {
            auto const m = static_cast<std::size_t>(std::lround(static_cast<double>(n) * std::exp2(k / 5.0)));
            auto table = Map(m);
            quiet += std::log(median_ns([&] {
                if constexpr (is_dense_ids<Map>::value) {
                    return workloads::find_dense_ids(&table);
                } else {
                    return workloads::find_all<true>(&table);
                }
            }));
            busy += std::log(median_ns([&] {
                if constexpr (is_dense_ids<Map>::value) {
                    return workloads::find_dense_ids_busy(&table, out);
                } else {
                    return workloads::find_hits_busy(&table, out);
                }
            }));
        }
        std::printf("%s %zu quiet %.3f\n", key, n, std::exp(quiet / 5));
        std::printf("%s %zu busy %.3f\n", key, n, std::exp(busy / 5));
    }
}

} // namespace

auto main() -> int {
    auto out = workloads::busy_sink();
    sizes<workloads::lookup_table<ankerl::unordered_dense::map<std::uint64_t, std::size_t>>>("uint64", &out);
    sizes<workloads::dense_id_table<ankerl::unordered_dense::map<std::uint64_t, std::size_t>>>("seq", &out);
#ifdef WITH_ID_MAP
    sizes<workloads::dense_id_table<idm::id_map<std::uint64_t, std::size_t>>>("seq_id_map", &out);
#endif
    sizes<workloads::lookup_table<ankerl::unordered_dense::map<std::string, std::size_t>>>("string", &out);
    return 0;
}
