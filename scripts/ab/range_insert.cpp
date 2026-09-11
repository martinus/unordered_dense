// What hashing ahead is worth to insert(first, last).
//
// A single insert has nothing to put between computing a key's hash and needing the block that hash
// points at. A range does: the hash of a later element, which depends on nothing the table is doing.
// `loop` inserts one at a time, `range` hands the whole thing to insert(first, last); before the
// pipelining landed the two were the same code.
//
// The map is rebuilt from empty every round, which is what a range insert is usually for, and the
// round is what is timed. `reserved` builds into a map that was told its size, so growth and the
// rehash -- which is pipelined already -- are out of the measurement and what is left is the insert.
//
//   argv: <loop|range> <n> [rounds] [reserved|grow]
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
#if defined(UDM_RI_STR)
using key_type = std::string;
#else
using key_type = std::uint64_t;
#endif
using map_t = ankerl::unordered_dense::map<key_type, std::size_t>;

struct rng {
    std::uint64_t s;
    explicit rng(std::uint64_t seed)
        : s(seed * UINT64_C(0x9E3779B97F4A7C15) | 1U) {}
    auto operator()() -> std::uint64_t {
        s ^= s << 13U;
        s ^= s >> 7U;
        s ^= s << 17U;
        return s;
    }
};
} // namespace

int main(int argc, char** argv) {
    workloads::tame_allocator();
    auto const how = std::string(argc > 1 ? argv[1] : "range");
    auto const n = static_cast<std::size_t>(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 200000);
    auto const rounds = static_cast<std::size_t>(argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 20);
    auto const reserved = std::string(argc > 4 ? argv[4] : "grow") == "reserved";

    // Built once, outside the timing: what is being measured is the insert, not making the input.
    auto input = std::vector<std::pair<key_type, std::size_t>>();
    input.reserve(n);
    auto r = rng(1);
    for (std::size_t i = 0; i < n; ++i) {
        input.emplace_back(workloads::key_for<map_t>(r() >> 2U), i);
    }

    auto acc = std::size_t{0};
    auto const started = std::chrono::steady_clock::now();
    for (std::size_t round = 0; round < rounds; ++round) {
        auto map = map_t();
        if (reserved) {
            map.reserve(n);
        }
        if (how == "range") {
            map.insert(input.begin(), input.end());
        } else {
            for (auto const& kv : input) {
                map.insert(kv);
            }
        }
        acc += map.size();
    }
    auto const elapsed = std::chrono::steady_clock::now() - started;
    std::printf("%.3f ns/element  %s n=%zu rounds=%zu %s acc=%zu\n",
                std::chrono::duration<double, std::nano>(elapsed).count() / static_cast<double>(n * rounds),
                how.c_str(),
                n,
                rounds,
                reserved ? "reserved" : "grow",
                acc);
}
