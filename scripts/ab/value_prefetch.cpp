// Can a hash-predictable value address be prefetched in time to matter?
//
// This map pays two dependent memory accesses on a hit: the group block, and then the value the
// slot's index points at. A flat map pays one, which is most of the 10-13% boost leads by on a
// fresh hit. The value's address cannot be predicted here, because the value vector is ordered by
// *insertion* and the map does not choose where a value goes -- that is the whole content of issue
// #229, and it is why a tiny pointer cannot shrink the index while that contract holds.
//
// A container that *did* own placement (TPHT-style: values in a hash-addressed table, the index
// holding a small offset from a base the group implies) would know the value's line before the
// group arrived, and could fetch both at once. Whether that actually recovers a memory latency on
// real hardware is the one thing about such a container that argument cannot settle: a software
// prefetch that misses the dTLB may be dropped, and at DRAM sizes the access may already be bound
// by bandwidth or translation rather than by latency.
//
// So this makes the prediction true by construction inside the *shipped* map, without building the
// container. Keys are inserted in home-group order, exactly twelve per group, so the value of slot
// j of group g sits at m_values[g * 12 + j] and the values of a group are twelve contiguous
// entries. The patched probe (see value_prefetch.sh) then prefetches `values + g * 12` before it
// touches the group. That is an upper bound on what the container could get -- perfect packing, no
// holes, no second choice -- which is what a kill experiment wants: if the bound does not clear the
// bar, the container cannot either.
//
//   argv: <hit|miss|half|none> <grouped|random> <p> [reps]     n = 12 * 2^p entries
//
// Built by scripts/ab/value_prefetch.sh, which is where the header patch and the variants live.
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef UDM_VP_PER_GROUP
#    define UDM_VP_PER_GROUP 12
#endif

namespace {

// Its own, so that the harness needs no nanobench object to link against -- probe_length.cpp does
// the same. Any full-period generator does; nothing here is measuring the generator.
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

#if defined(UDM_VP_STR)
using key_type = std::string;
#elif defined(UDM_VP_BIG)
using key_type = std::uint64_t;
#else
using key_type = std::uint64_t;
#endif

#if defined(UDM_VP_BIG)
using mapped_type = workloads::big_value;
#else
using mapped_type = std::size_t;
#endif

using map_t = ankerl::unordered_dense::map<key_type, mapped_type>;

// The group a key would call home, computed the way the table does: the map's own hash finalized,
// then its top `p` bits. Both hashes this harness uses are avalanching, so mixed_hash is the hash
// itself and this is exactly group_idx_from_hash.
auto home_group(std::uint64_t v, unsigned p) -> std::size_t {
    auto const h = ankerl::unordered_dense::hash<key_type>{}(workloads::key_for<map_t>(v));
    return static_cast<std::size_t>(h >> (64U - p));
}

// `per_group` keys for each of the 2^p groups, laid out [g * per_group + j], drawn from `range`.
auto keys_by_group(unsigned p, std::size_t per_group, std::uint64_t range) -> std::vector<std::uint64_t> {
    auto const groups = std::size_t{1} << p;
    auto out = std::vector<std::uint64_t>(groups * per_group);
    auto filled = std::vector<std::uint8_t>(groups, 0);
    auto remaining = groups;
    auto r = rng(range == 0 ? 1 : 2);
    while (remaining != 0) {
        auto const v = (r() >> 2U) | range;
        auto const g = home_group(v, p);
        if (filled[g] == per_group) {
            continue;
        }
        out[g * per_group + filled[g]] = v;
        if (++filled[g] == per_group) {
            --remaining;
        }
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    workloads::tame_allocator();
    auto const what = std::string(argc > 1 ? argv[1] : "hit");
    auto const order = std::string(argc > 2 ? argv[2] : "grouped");
    auto const p = static_cast<unsigned>(argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 14);
    auto const reps = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 30000000;

    constexpr auto per_group = std::size_t{UDM_VP_PER_GROUP};
    auto const groups = std::size_t{1} << p;
    auto const n = groups * per_group;

    auto present = keys_by_group(p, per_group, 0);
    auto absent = keys_by_group(p, per_group, std::uint64_t{1} << 63U);

    auto map = map_t();
    map.reserve(n);
    // reserve must give exactly the group count the layout was computed for, or the home group of
    // every key is a different number than the one the prefetch address is built from.
    if (map.bucket_count() / 16U != groups) {
        std::fprintf(stderr, "reserve(%zu) gave %zu groups, expected %zu\n", n, map.bucket_count() / 16U, groups);
        return 1;
    }

    if (order == "grouped") {
        for (auto const v : present) {
            map.try_emplace(workloads::key_for<map_t>(v), mapped_type{});
        }
    } else {
        // the same keys and the same table, in an order that puts no value where its group implies
        auto shuffled = present;
        auto r = rng(77);
        for (std::size_t i = shuffled.size(); i > 1; --i) {
            std::swap(shuffled[i - 1], shuffled[r.bounded(i)]);
        }
        for (auto const v : shuffled) {
            map.try_emplace(workloads::key_for<map_t>(v), mapped_type{});
        }
    }
    if (map.size() != n || map.bucket_count() / 16U != groups) {
        std::fprintf(stderr, "filling changed the table: size %zu of %zu, %zu groups\n", map.size(), n, map.bucket_count() / 16U);
        return 1;
    }

    // The invariant the whole experiment rests on: in grouped order the value of a key is exactly
    // where its home group says it is. Checked on a sample rather than asserted in a comment.
    if (order == "grouped") {
        auto r = rng(4);
        for (auto i = 0; i < 1000; ++i) {
            auto const at = r.bounded(n);
            auto const it = map.find(workloads::key_for<map_t>(present[at]));
            if (it == map.end() || static_cast<std::size_t>(it - map.begin()) != at) {
                std::fprintf(stderr, "grouped layout is not exact at %zu\n", at);
                return 1;
            }
        }
    }

    auto acc = std::size_t{0};
    // The rngs outlive the loop: a small batch replayed is learned by the branch predictor, which
    // has flattered the branchiest probe by 2.7x once already.
    auto r = rng(5);
    auto coin = rng(99);
    if (what == "none") {
        for (std::size_t i = 0; i < reps; ++i) {
            auto const at = r.bounded(n);
            acc += present[at] & 1U;
        }
    } else {
        for (std::size_t i = 0; i < reps; ++i) {
            auto const at = r.bounded(n);
            auto const hit = what == "hit" || (what == "half" && (coin() & 1U) != 0);
            acc += map.count(workloads::key_for<map_t>(hit ? present[at] : absent[at]));
        }
    }
    std::printf("%s %s p=%u n=%zu reps=%zu acc=%zu\n", what.c_str(), order.c_str(), p, n, reps, acc);
}
