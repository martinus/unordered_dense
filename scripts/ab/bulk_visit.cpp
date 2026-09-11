// Three ways to look up a batch of keys, doing identical work, so the difference is the pipeline.
//
//   inline    the key is fetched and looked up in one loop body
//   plain     the keys are materialised into a batch first, then looked up one at a time
//   bulk      the same batch handed to visit()
//
// `inline` against `plain` is the one that surprised: **simply materialising the batch is worth
// 1.5x** at four million entries, because two loops each saturate their own memory parallelism
// where one long body does not. Any claim about a lookup-pipelining feature has to be measured
// against `plain`, not against `inline`, or it collects that 1.5x for free -- which is exactly how
// an earlier prefetch(key) API came to be documented at 1.5x and then reverted, since in a batched
// loop it was a small loss.
//
// All three sum the mapped values of the keys they find, so none is doing less work than another,
// and all three must print the same checksum. The loop times itself, so no perf is needed.
//
//   argv: <hit|half> <inline|plain|bulk> <n> [reps]
//
// Sweep `n` and sweep `hit` against `half`: whether visit()'s chunking is repaid depends on both the
// map's size and the caller's hit rate, and at small n those two disagree in sign. One point
// measurement here says nothing; the numbers are in notes/index-design.md under "The other two
// pipelines".
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

#if defined(UDM_BV_STR)
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
    auto bounded(std::size_t n) -> std::size_t {
        return static_cast<std::size_t>((((*this)() >> 32U) * n) >> 32U);
    }
};

// How many keys the bulk call is handed at a time. Bigger than the map's own chunk on purpose: the
// caller's batch and the map's pipeline depth are different things, and a caller should not have to
// know the second one.
constexpr std::size_t batch = 256;

} // namespace

int main(int argc, char** argv) {
    workloads::tame_allocator();
    auto const what = std::string(argc > 1 ? argv[1] : "hit");
    auto const how = std::string(argc > 2 ? argv[2] : "plain");
    auto const n = static_cast<std::size_t>(argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 4000000);
    auto const reps = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 4000000;

    auto present = std::vector<std::uint64_t>();
    auto absent = std::vector<std::uint64_t>();
    auto r = rng(1);
    for (std::size_t i = 0; i < n; ++i) {
        present.push_back(r() >> 2U);
    }
    for (std::size_t i = 0; i < n; ++i) {
        absent.push_back((r() >> 2U) | (std::uint64_t{1} << 62U));
    }

    auto map = map_t();
    map.reserve(n);
    for (auto v : present) {
        map.try_emplace(workloads::key_for<map_t>(v), static_cast<std::size_t>(v & 0xFFU));
    }

    auto pick = rng(5);
    auto coin = rng(99);
    auto hits_only = what == "hit";
    auto next_key = [&]() -> std::uint64_t {
        auto const at = pick.bounded(n);
        return (hits_only || (coin() & 1U) != 0) ? present[at] : absent[at];
    };

    auto acc = std::size_t{0};
    auto const started = std::chrono::steady_clock::now();

    // `bulk` and `plain` both work from a materialised batch; only the lookup differs, so the fill
    // is written once and charged to both.
    auto keys = std::vector<key_type>(batch);
    auto fill = [&](std::size_t m) {
        for (std::size_t i = 0; i < m; ++i) {
            keys[i] = workloads::key_for<map_t>(next_key());
        }
    };
    auto const chunk = [&](std::size_t done) {
        return (reps - done) < batch ? reps - done : batch;
    };

    if (how == "bulk") {
        for (std::size_t done = 0; done < reps; done += batch) {
            auto const m = chunk(done);
            fill(m);
            map.visit(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(m), [&](auto const& kv) {
                acc += kv.second;
            });
        }
    } else if (how == "inline") {
        // The key is acquired and looked up in one loop body, so the key's own cache miss sits in
        // front of the map's and the two cannot overlap across iterations. This is the shape a
        // pipelining feature flatters, and it is not the shape a caller with a batch of keys is in.
        for (std::size_t i = 0; i < reps; ++i) {
            auto const it = map.find(workloads::key_for<map_t>(next_key()));
            if (it != map.end()) {
                acc += it->second;
            }
        }
    } else {
        for (std::size_t done = 0; done < reps; done += batch) {
            auto const m = chunk(done);
            fill(m);
            for (std::size_t i = 0; i < m; ++i) {
                auto const it = map.find(keys[i]);
                if (it != map.end()) {
                    acc += it->second;
                }
            }
        }
    }

    auto const elapsed = std::chrono::steady_clock::now() - started;
    std::printf("%.3f ns/lookup  %s %s n=%zu reps=%zu acc=%zu\n",
                std::chrono::duration<double, std::nano>(elapsed).count() / static_cast<double>(reps),
                what.c_str(),
                how.c_str(),
                n,
                static_cast<std::size_t>(reps),
                acc);
}
