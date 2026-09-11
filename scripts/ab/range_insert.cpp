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
//   argv: <loop|range> <n> [rounds] [reserved|grow] [dup-percent] [vector|list]
//
// `dup-percent` is the share of the input that repeats an earlier key -- the case where reserving
// from std::distance over-allocates, since the range is longer than the map will end up being. The
// source container matters too: a std::list is a forward range, so std::distance over it is O(n) and
// walks the whole thing a second time, which is the case where asking how long the range is may cost
// more than knowing. The final bucket_count is printed so over-allocation is visible and not just
// inferred.
//
// `loop` is the right control for "what does the range overload buy a caller" and the wrong one for
// "does the ring inside it pay": it also carries the call boundary's ~15 instructions per element,
// so it shows the range winning at every size and hides any pipeline penalty underneath. For the
// second question build the header twice, once with the ring taken out of do_insert_range, and run
// `range` against `range`. Sweep `grow` as well as `reserved` -- they part company below a few
// thousand elements.
//
// Built by hand; -DUDM_RI_STR swaps the key for a std::string, which is the shape where the hash
// being pipelined is worth the most.
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <list>
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
    auto const dup_pct = static_cast<std::size_t>(argc > 5 ? std::strtoull(argv[5], nullptr, 10) : 0);
    auto const as_list = std::string(argc > 6 ? argv[6] : "vector") == "list";

    // Built once, outside the timing: what is being measured is the insert, not making the input.
    auto input = std::vector<std::pair<key_type, std::size_t>>();
    input.reserve(n);
    auto r = rng(1);
    auto distinct = std::vector<std::uint64_t>();
    for (std::size_t i = 0; i < n; ++i) {
        auto const dup = !distinct.empty() && (r() % 100) < dup_pct;
        auto const v = dup ? distinct[static_cast<std::size_t>(r() % distinct.size())] : (r() >> 2U);
        if (!dup) {
            distinct.push_back(v);
        }
        input.emplace_back(workloads::key_for<map_t>(v), i);
    }
    auto const listed = std::list<std::pair<key_type, std::size_t>>(input.begin(), input.end());

    auto acc = std::size_t{0};
    auto buckets = std::size_t{0};
    auto const started = std::chrono::steady_clock::now();
    for (std::size_t round = 0; round < rounds; ++round) {
        auto map = map_t();
        if (reserved) {
            map.reserve(n);
        }
        if (how == "range") {
            if (as_list) {
                map.insert(listed.begin(), listed.end());
            } else {
                map.insert(input.begin(), input.end());
            }
        } else if (as_list) {
            for (auto const& kv : listed) {
                map.insert(kv);
            }
        } else {
            for (auto const& kv : input) {
                map.insert(kv);
            }
        }
        acc += map.size();
        buckets = map.bucket_count();
    }
    auto const elapsed = std::chrono::steady_clock::now() - started;
    std::printf("%.3f ns/element  %s n=%zu rounds=%zu %s dup=%zu%% %s size=%zu buckets=%zu acc=%zu\n",
                std::chrono::duration<double, std::nano>(elapsed).count() / static_cast<double>(n * rounds),
                how.c_str(),
                n,
                rounds,
                reserved ? "reserved" : "grow",
                dup_pct,
                as_list ? "list" : "vector",
                acc / rounds,
                buckets,
                acc);
}
