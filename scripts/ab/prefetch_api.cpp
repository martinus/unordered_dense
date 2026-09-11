// What prefetch(key) is worth to a caller with independent lookups.
//
// A lookup on a table past the cache is two dependent memory accesses -- the group block, then the
// value -- and nothing inside a single lookup can overlap them. A caller looking up *many* keys can
// overlap them across lookups: ask for the block of the key wanted `depth` iterations from now,
// then do the lookup whose block was asked for `depth` iterations ago. Nothing gets faster; several
// misses are simply outstanding at once.
//
// This measures that, against the same loop with no prefetching, at sizes from inside L2 to well
// past L3. depth 0 is the baseline, so both sides run the identical loop and the only difference is
// the prefetch.
//
// The loop times itself and prints ns per lookup, so nothing outside it is counted and the harness
// needs no perf -- which matters because the machines worth asking this on include macOS, where
// there is none. An earlier version measured the whole process under `perf stat` and subtracted a
// shorter run to cancel the setup; timing the loop directly is both simpler and exact.
//
// Pipelining the loop moves three things earlier at once: the key's own fetch, its hash, and the
// block prefetch. Only the third is what prefetch() adds, so `mode` separates them -- `hash` fills
// the ring with hash_for(), which pipelines the first two and touches no map memory, and `prefetch`
// fills it with prefetch(). The difference between those two columns is the prefetch and nothing
// else.
//
// The `hash` column is **not** a speedup to compare against depth 0; read it as the price of the
// pipeline itself. Deferring a lookup costs a ring slot and a second key_for(), and buys nothing
// unless something is fetched early -- so on an all-hits run it comes out *slower* than no
// pipelining at all, by 7% here and considerably more on a Neoverse. That is the bar the prefetch
// has to clear, which is why it is the column the prefetch is measured against.
//
//   argv: <hit|half> <depth> <n> [reps] [prefetch|hash]
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

#if defined(UDM_PF_STR)
using key_type = std::string;
#else
using key_type = std::uint64_t;
#endif
using map_t = ankerl::unordered_dense::map<key_type, std::size_t>;
using hash_t = map_t::precomputed_hash;

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

// The deepest pipeline the ring allows. Eight is best on a Zen 4, which holds about two dozen
// misses outstanding; a core with more memory parallelism can want considerably more, so the sweep
// has to be able to ask for more than the answer on one machine.
constexpr std::size_t max_depth = 128;

} // namespace

int main(int argc, char** argv) {
    workloads::tame_allocator();
    auto const what = std::string(argc > 1 ? argv[1] : "hit");
    auto const depth = static_cast<std::size_t>(argc > 2 ? std::strtoul(argv[2], nullptr, 10) : 0);
    auto const n = static_cast<std::size_t>(argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1000000);
    auto const reps = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 20000000;
    auto const mode = std::string(argc > 5 ? argv[5] : "prefetch");
    if (depth > max_depth) {
        std::fprintf(stderr, "depth %zu above the ring's %zu\n", depth, max_depth);
        return 1;
    }

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
        map.try_emplace(workloads::key_for<map_t>(v), std::size_t{1});
    }

    // Keys are drawn inside the loop rather than into a vector sized by `reps`: such a vector's
    // own cost is proportional to the repetition count and would survive the slope subtraction,
    // which only removes what is fixed. Drawing also keeps the sequence from being replayed, which
    // a branch predictor learns.
    // What goes into the ring: hash_for pipelines the key fetch and the hash and touches no map
    // memory, prefetch also fetches the block. The difference between the two is the prefetch.
    //
    // Decided once rather than compared per iteration. A std::string compare in the inner loop is
    // worth under a percent here and is not necessarily the same price on another core, which
    // matters for a harness whose whole job is comparing cores.
    auto const hash_only = mode == "hash";
    auto const ahead = [&](key_type const& k) -> hash_t { return hash_only ? map.hash_for(k) : map.prefetch(k); };

    auto pick = rng(5);
    auto coin = rng(99);
    auto next_key = [&]() -> std::uint64_t {
        auto const at = pick.bounded(n);
        auto const hit = what == "hit" || (coin() & 1U) != 0;
        return hit ? present[at] : absent[at];
    };

    auto acc = std::size_t{0};
    auto const started = std::chrono::steady_clock::now();
    if (depth == 0) {
        for (std::size_t i = 0; i < reps; ++i) {
            acc += map.count(workloads::key_for<map_t>(next_key()));
        }
    } else {
        // Both the key and its hash: count() needs the key to compare and the hash to probe with.
        auto keys = std::vector<std::uint64_t>(max_depth);
        auto ring = std::vector<hash_t>(max_depth);
        for (std::size_t i = 0; i < depth; ++i) {
            keys[i] = next_key();
            ring[i] = ahead(workloads::key_for<map_t>(keys[i]));
        }
        for (std::size_t i = 0; i < reps; ++i) {
            // this lookup's key and hash first, then refill the slot they free: the slot holding
            // key i is the one key i + depth goes into
            auto const slot = i % depth;
            auto const v = keys[slot];
            auto const ph = ring[slot];
            keys[slot] = next_key();
            ring[slot] = ahead(workloads::key_for<map_t>(keys[slot]));
            acc += map.count(workloads::key_for<map_t>(v), ph);
        }
    }
    auto const elapsed = std::chrono::steady_clock::now() - started;
    auto const ns = std::chrono::duration<double, std::nano>(elapsed).count() / static_cast<double>(reps);
    std::printf("%.3f ns/lookup  %s depth=%zu n=%zu reps=%zu mode=%s acc=%zu\n",
                ns, what.c_str(), depth, n, reps, mode.c_str(), acc);
}
