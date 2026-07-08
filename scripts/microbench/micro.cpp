// Standalone micro harness mirroring the bench_quick_overall_udm workloads
// (see test/bench/quick_overall_map.cpp — same rng seeds, same iteration counts).
//
// Each invocation runs ONE workload `reps` times in-process and prints the
// fastest run in milliseconds (min-of-reps is robust against noise spikes on
// shared machines). Results are verified against known checksums, so a broken
// "optimization" fails loudly instead of measuring garbage.
//
// Build (a few seconds, no meson needed):
//   clang++ -O3 -DNDEBUG -std=c++17 -I<repo>/include -I<repo>/test \
//       micro.cpp nanobench_impl.cpp -o micro
//
// Usage:
//   ./micro <ie64|iestr|find64|findstr|it64|itstr> [reps]
//
// Normally driven by ab.sh (same directory), which builds a baseline binary
// from a git ref and a candidate from the working tree and interleaves them.

#include <ankerl/unordered_dense.h>

#include <third-party/nanobench.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

template <typename K>
inline auto init_key() -> K {
    return {};
}

template <typename T>
inline void randomize_key(ankerl::nanobench::Rng* rng, int n, T* key) {
    // we limit ourselves to 32bit n
    auto limited = (((*rng)() >> 32U) * static_cast<uint64_t>(n)) >> 32U;
    *key = static_cast<T>(limited);
}

template <>
[[nodiscard]] inline auto init_key<std::string>() -> std::string {
    std::string str;
    str.resize(200);
    return str;
}

inline void randomize_key(ankerl::nanobench::Rng* rng, int n, std::string* key) {
    uint64_t k{};
    randomize_key(rng, n, &k);
    std::memcpy(key->data(), &k, sizeof(k));
}

// mirrors bench_random_insert_erase
template <typename Map>
auto insert_erase() -> uint64_t {
    ankerl::nanobench::Rng rng(123);
    size_t verifier{};
    Map map;
    auto key = init_key<typename Map::key_type>();
    for (int n = 1; n < 20000; ++n) {
        for (int i = 0; i < 200; ++i) {
            randomize_key(&rng, n, &key);
            map[key];
            randomize_key(&rng, n, &key);
            verifier += map.erase(key);
        }
    }
    return verifier + map.size();
}

// mirrors bench_random_find (50% hit rate)
template <typename Map>
auto find_50() -> uint64_t {
    uint64_t const seed = 123123;
    ankerl::nanobench::Rng numbers_insert_rng(seed);
    size_t numbers_insert_rng_calls = 0;
    ankerl::nanobench::Rng numbers_search_rng(seed);
    size_t numbers_search_rng_calls = 0;
    ankerl::nanobench::Rng insertion_rng(123);
    size_t checksum = 0;
    Map map;
    auto key = init_key<typename Map::key_type>();
    for (size_t i = 0; i < 100000; ++i) {
        randomize_key(&numbers_insert_rng, 1000000, &key);
        ++numbers_insert_rng_calls;
        if (insertion_rng() & 1U) {
            map[key] = i;
        }
        for (size_t search = 0; search < 100; ++search) {
            randomize_key(&numbers_search_rng, 1000000, &key);
            ++numbers_search_rng_calls;
            auto it = map.find(key);
            if (it != map.end()) {
                checksum += it->second;
            }
            if (numbers_insert_rng_calls == numbers_search_rng_calls) {
                numbers_search_rng = ankerl::nanobench::Rng(seed);
                numbers_search_rng_calls = 0;
            }
        }
    }
    return checksum;
}

// mirrors bench_iterate
template <typename Map>
auto iterate() -> uint64_t {
    size_t const num_elements = 5000;
    auto key = init_key<typename Map::key_type>();
    ankerl::nanobench::Rng rng(555);
    Map map;
    size_t result = 0;
    for (size_t n = 0; n < num_elements; ++n) {
        randomize_key(&rng, 1000000, &key);
        map[key] = n;
        for (auto const& key_val : map) {
            result += key_val.second;
        }
    }
    rng = ankerl::nanobench::Rng(555);
    do {
        randomize_key(&rng, 1000000, &key);
        map.erase(key);
        for (auto const& key_val : map) {
            result += key_val.second;
        }
    } while (!map.empty());
    return result;
}

using map_u64 = ankerl::unordered_dense::map<uint64_t, size_t>;
using map_str = ankerl::unordered_dense::map<std::string, size_t, ankerl::unordered_dense::hash<std::string>>;

// expected results; all are independent of the hash function (counts and
// order-independent sums), so they hold across hash algorithm changes
constexpr uint64_t expected_ie = 1994641U + 9987U;
constexpr uint64_t expected_find = 130681773795U;
constexpr uint64_t expected_it = 62282755409U;

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 2) {
        printf("usage: %s <ie64|iestr|find64|findstr|it64|itstr> [reps]\n", argv[0]);
        return 1;
    }
    char const* workload = argv[1];
    int reps = argc > 2 ? atoi(argv[2]) : 1;
    double best = 1e30;
    for (int r = 0; r < reps; ++r) {
        uint64_t result = 0;
        uint64_t expected = 0;
        auto t0 = std::chrono::steady_clock::now();
        if (0 == strcmp(workload, "ie64")) {
            result = insert_erase<map_u64>();
            expected = expected_ie;
        } else if (0 == strcmp(workload, "iestr")) {
            result = insert_erase<map_str>();
            expected = expected_ie;
        } else if (0 == strcmp(workload, "find64")) {
            result = find_50<map_u64>();
            expected = expected_find;
        } else if (0 == strcmp(workload, "findstr")) {
            result = find_50<map_str>();
            expected = expected_find;
        } else if (0 == strcmp(workload, "it64")) {
            result = iterate<map_u64>();
            expected = expected_it;
        } else if (0 == strcmp(workload, "itstr")) {
            result = iterate<map_str>();
            expected = expected_it;
        } else {
            fprintf(stderr, "unknown workload '%s'\n", workload);
            return 1;
        }
        auto t1 = std::chrono::steady_clock::now();
        if (result != expected) {
            fprintf(stderr,
                    "RESULT MISMATCH for %s: got %llu, expected %llu — the map is broken, timing is meaningless\n",
                    workload,
                    static_cast<unsigned long long>(result),
                    static_cast<unsigned long long>(expected));
            return 1;
        }
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms < best) {
            best = ms;
        }
    }
    printf("%.1f\n", best);
    return 0;
}
