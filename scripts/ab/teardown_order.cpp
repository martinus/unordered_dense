// Builds a segmented_map from empty, destroys it and builds it again in one process, ROUNDS times,
// and prints the first build (on a fresh heap), the median of the warm builds after it, the median
// teardown, and the median of what glibc handed back to the kernel at each teardown (the main arena
// before and after, from mallinfo2). Times in ms. One key kind per binary:
//
//   KIND_U64    segmented_map<uint64_t, uint64_t>
//   KIND_STR    segmented_map<std::string, uint64_t>
//   KIND_OWNED  segmented_map<uint64_t, std::unique_ptr<std::array<std::uint8_t, 96>>>: one allocation
//               of the caller's per entry, interleaved with the map's own, which is Bonxai's root
//               map reduced to the map
//
// Keys are the workloads' (test/bench/workloads.h). Run it through scripts/ab/teardown_order.sh.
//
//   ./teardown_order ROUNDS SIZE
#include <ankerl/unordered_dense.h>
#include <bench/workloads.h>

#include <malloc.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

#if defined(KIND_U64)
using key_type = std::uint64_t;
using mapped_type = std::uint64_t;
constexpr char const* kind = "u64";
auto value_at(std::size_t i) -> mapped_type {
    return i;
}
#elif defined(KIND_STR)
using key_type = std::string;
using mapped_type = std::uint64_t;
constexpr char const* kind = "str";
auto value_at(std::size_t i) -> mapped_type {
    return i;
}
#elif defined(KIND_OWNED)
using key_type = std::uint64_t;
using mapped_type = std::unique_ptr<std::array<std::uint8_t, 96>>;
constexpr char const* kind = "owned";
auto value_at(std::size_t i) -> mapped_type {
    auto p = std::make_unique<std::array<std::uint8_t, 96>>();
    (*p)[0] = static_cast<std::uint8_t>(i);
    return p;
}
#else
#    error "define KIND_U64, KIND_STR or KIND_OWNED"
#endif

using map_t = ankerl::unordered_dense::segmented_map<key_type, mapped_type>;
using clk = std::chrono::steady_clock;

auto ms(clk::duration d) -> double {
    return std::chrono::duration<double, std::milli>(d).count();
}

auto median(std::vector<double> v) -> double {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

volatile std::size_t sink = 0;

} // namespace

int main(int argc, char** argv) {
    auto const rounds = argc == 3 ? std::atoi(argv[1]) : 0;
    if (rounds < 2) {
        std::fprintf(stderr, "usage: %s ROUNDS SIZE, ROUNDS >= 2\n", argv[0]);
        return 1;
    }
    auto const n = static_cast<std::size_t>(std::atol(argv[2]));
    auto keys = std::vector<key_type>();
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        keys.emplace_back(workloads::key_source<key_type>::get(i)); // a copy: the string source reuses its buffer
    }
    auto builds = std::vector<double>();
    auto teardowns = std::vector<double>();
    auto released = std::vector<double>();
    for (int round = 0; round < rounds; ++round) {
        auto* m = new map_t();
        auto const t0 = clk::now();
        for (std::size_t i = 0; i < n; ++i) {
            m->try_emplace(keys[i], value_at(i));
        }
        auto const t1 = clk::now();
        sink = sink + m->size();
        auto const arena_before = mallinfo2().arena;
        auto const t2 = clk::now();
        delete m;
        auto const t3 = clk::now();
        auto const arena_after = mallinfo2().arena;
        builds.push_back(ms(t1 - t0));
        teardowns.push_back(ms(t3 - t2));
        released.push_back(static_cast<double>(arena_before - arena_after) / 1048576.0);
    }
    auto const warm = std::vector<double>(builds.begin() + 1, builds.end());
    std::printf("%s n=%zu first_build %.3f warm_build %.3f teardown %.3f released_mb %.1f\n",
                kind,
                n,
                builds.front(),
                median(warm),
                median(teardowns),
                median(released));
    return 0;
}
