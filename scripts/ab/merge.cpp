// What merge() is worth over the loop a caller would otherwise write.
//
// `loop` is that loop: try_emplace here, and on success erase(it) there. It is correct, and it pays
// the source's index twice for every element it takes -- a second hash of the key leaving, and a
// third of whichever element the backfill drags into the hole. `merge` takes the same elements and
// repairs the source once at the end instead, by compacting what stayed behind and rebuilding its
// index over that.
//
// The two maps are rebuilt from the same inputs every round because a merge consumes both of them;
// only the merge itself is inside the clock. The overlap is the axis that matters: at 0% every
// element moves and the source ends empty, at 100% nothing moves and both versions do the same
// probes and no writes at all.
//
//   argv: <loop|merge> <n> [rounds] [overlap-percent] [reserve]
//
// `reserve` calls reserve(size() + source.size()) before the merge, inside the clock: the merge
// cannot end up larger than that, and it is the only thing about the result the map knows before it
// has probed for it.
//
// n is the source's size; the destination is built to the same size. Built by hand;
// -DUDM_MERGE_STR swaps the key for a std::string, which is where re-hashing a key twice costs the
// most.
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {
#if defined(UDM_MERGE_STR)
using key_type = std::string;
#else
using key_type = std::uint64_t;
#endif
using map_t = ankerl::unordered_dense::map<key_type, std::size_t>;

auto fill(std::vector<std::pair<key_type, std::size_t>> const& from) -> map_t {
    auto map = map_t();
    map.insert(from.begin(), from.end());
    return map;
}
} // namespace

int main(int argc, char** argv) {
    workloads::tame_allocator();
    auto const how = std::string(argc > 1 ? argv[1] : "merge");
    auto const n = static_cast<std::size_t>(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 200000);
    auto const rounds = static_cast<std::size_t>(argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 20);
    auto const overlap_pct = static_cast<std::size_t>(argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 0);
    auto const with_reserve = std::string(argc > 5 ? argv[5] : "") == "reserve";

    // Two key ranges that share exactly overlap_pct of the source: the destination holds
    // [0, n), the source [n - shared, 2n - shared).
    auto const shared = n * overlap_pct / 100;
    auto dest_input = std::vector<std::pair<key_type, std::size_t>>();
    auto src_input = std::vector<std::pair<key_type, std::size_t>>();
    for (std::size_t i = 0; i < n; ++i) {
        dest_input.emplace_back(workloads::key_for<map_t>(i * UINT64_C(0x9E3779B97F4A7C15)), i);
        src_input.emplace_back(workloads::key_for<map_t>((i + n - shared) * UINT64_C(0x9E3779B97F4A7C15)), i);
    }

    auto acc = std::size_t{0};
    auto elapsed = std::chrono::steady_clock::duration{};
    for (std::size_t round = 0; round < rounds; ++round) {
        auto dest = fill(dest_input);
        auto src = fill(src_input);

        auto const started = std::chrono::steady_clock::now();
        if (with_reserve) {
            dest.reserve(dest.size() + src.size());
        }
        if (how == "merge") {
            dest.merge(src);
        } else {
            for (auto it = src.begin(); it != src.end();) {
                // The key is copied, not moved, and it has to be: erase(it) hashes the key to find
                // the slot pointing at it, so a caller that moves the key out first hands erase() a
                // moved-from key and the probe for a slot that is not on that sequence does not
                // return. Which is a cost of writing this by hand -- one copy of every key taken --
                // and the only way around it is extract(it), which hashes the key a third time.
                if (dest.try_emplace(it->first, std::move(it->second)).second) {
                    it = src.erase(it);
                } else {
                    ++it;
                }
            }
        }
        elapsed += std::chrono::steady_clock::now() - started;
        acc += dest.size() * 3 + src.size();
    }
    std::printf("%.3f ns/element  %s n=%zu rounds=%zu overlap=%zu%% acc=%zu\n",
                std::chrono::duration<double, std::nano>(elapsed).count() / static_cast<double>(n * rounds),
                how.c_str(),
                n,
                rounds,
                overlap_pct,
                acc / rounds);
}
