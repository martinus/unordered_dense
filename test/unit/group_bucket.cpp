#include <ankerl/unordered_dense.h>

#include <app/counter.h>
#include <app/doctest.h>

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// The group index, bucket_type::group: sixteen fingerprints and eight overflow counters per
// group, value indices beside them. Every TEST_CASE_MAP in the suite runs on it too, through
// group_map in app/doctest.h; what is here is what only makes sense for this index.

using group_map_t = ankerl::unordered_dense::map<std::string,
                                                 size_t,
                                                 ankerl::unordered_dense::hash<std::string>,
                                                 std::equal_to<std::string>,
                                                 std::allocator<std::pair<std::string, size_t>>,
                                                 ankerl::unordered_dense::bucket_type::group>;

using group_big_map_t = ankerl::unordered_dense::map<std::string,
                                                     size_t,
                                                     ankerl::unordered_dense::hash<std::string>,
                                                     std::equal_to<std::string>,
                                                     std::allocator<std::pair<std::string, size_t>>,
                                                     ankerl::unordered_dense::bucket_type::group_big>;

// 24 bytes per sixteen slots; the value indices are not in the group
static_assert(sizeof(group_map_t::bucket_type) == 24U);
static_assert(sizeof(group_big_map_t::bucket_type) == 24U);
static_assert(group_map_t::max_size() == group_map_t::max_bucket_count());

#if SIZE_MAX == UINT32_MAX
static_assert(group_map_t::max_size() == uint64_t{1} << 31U);
static_assert(group_big_map_t::max_size() == uint64_t{1} << 31U);
#else
static_assert(group_map_t::max_size() == uint64_t{1} << 32U);
static_assert(group_big_map_t::max_size() == uint64_t{1} << 63U);
#endif

// max_size() is a multiple of sixteen, so the largest array is whole groups
static_assert(group_map_t::max_bucket_count() % 16U == 0U);
static_assert(group_big_map_t::max_bucket_count() % 16U == 0U);

TEST_CASE("group_bucket_count_is_in_slots") {
    auto map = group_map_t();
    REQUIRE(map.bucket_count() == 0U);
    map["a"] = 1;
    // the smallest array: four groups
    REQUIRE(map.bucket_count() == 64U);
    REQUIRE(map.bucket_count() % 16U == 0U);
    map.reserve(1000);
    REQUIRE(map.bucket_count() >= 1000U / map.max_load_factor());
    REQUIRE(map.bucket_count() % 16U == 0U);
    REQUIRE(map.load_factor() < map.max_load_factor());
}

// Against std::unordered_map, with every operation that touches the index: the overflow counters
// have to come back down on erase, or a later probe walks on past the group it should have
// stopped in and a later insert lands where a probe cannot find it. A run that grows and one
// that reserves first, so both the growth rehash and a table that never grows are covered.
namespace {

template <typename Map>
void run_against_reference(unsigned seed, uint64_t range, size_t ops, bool reserve_first) {
    auto rng = std::mt19937_64(seed);
    auto map = Map();
    auto ref = std::unordered_map<uint64_t, uint64_t>();
    if (reserve_first) {
        map.reserve(static_cast<size_t>(range / 2));
    }
    for (size_t op = 0; op < ops; ++op) {
        auto const key = rng() % range;
        switch (rng() % 8U) {
        case 0:
        case 1: {
            REQUIRE(map.try_emplace(key, op).second == ref.try_emplace(key, op).second);
            break;
        }
        case 2: {
            REQUIRE(map.erase(key) == ref.erase(key));
            break;
        }
        case 3: {
            // erase by iterator, which locates the slot from the value rather than from a probe
            auto it = map.find(key);
            if (it != map.end()) {
                map.erase(it);
                ref.erase(key);
            }
            break;
        }
        case 4: {
            if (op % 1000 == 0) {
                auto copy = map;
                map = std::move(copy);
            }
            break;
        }
        case 5: {
            if (op % 5000 == 0) {
                map.rehash(0);
            }
            break;
        }
        case 6: {
            // replace_key moves an entry between probe sequences without touching the values
            auto it = map.find(key);
            if (it != map.end()) {
                auto const new_key = key + range;
                auto const r = map.replace_key(it, new_key);
                if (r.second) {
                    auto const v = ref.at(key);
                    ref.erase(key);
                    ref[new_key] = v;
                }
            }
            break;
        }
        default: {
            auto a = map.find(key);
            auto b = ref.find(key);
            REQUIRE((a != map.end()) == (b != ref.end()));
            if (a != map.end()) {
                REQUIRE(a->second == b->second);
            }
            break;
        }
        }
        REQUIRE(map.size() == ref.size());
    }
    for (auto const& kv : map) {
        auto it = ref.find(kv.first);
        REQUIRE(it != ref.end());
        REQUIRE(it->second == kv.second);
    }
    for (auto const& kv : ref) {
        auto it = map.find(kv.first);
        REQUIRE(it != map.end());
        REQUIRE(it->second == kv.second);
    }
}

template <typename Bucket>
using u64_map = ankerl::unordered_dense::map<uint64_t,
                                             uint64_t,
                                             ankerl::unordered_dense::hash<uint64_t>,
                                             std::equal_to<uint64_t>,
                                             std::allocator<std::pair<uint64_t, uint64_t>>,
                                             Bucket>;

} // namespace

TEST_CASE("group_index_against_reference") {
    run_against_reference<u64_map<ankerl::unordered_dense::bucket_type::group>>(1, 10, 20000, false);
    run_against_reference<u64_map<ankerl::unordered_dense::bucket_type::group>>(2, 3000, 200000, false);
    run_against_reference<u64_map<ankerl::unordered_dense::bucket_type::group>>(3, 100000, 300000, true);
    run_against_reference<u64_map<ankerl::unordered_dense::bucket_type::group_big>>(4, 3000, 200000, false);
}

// A table that only churns. Every insert into a full group counts itself in that group's
// counter and every erase counts itself out again, so after every key has been erased the
// index is exactly what a fresh one is, and the same keys go back in as if for the first time.
// If a counter were left behind, the second round would still work -- a stale counter only
// makes a probe walk further -- so this asks the one thing that is observable from outside:
// that the table stays correct across many rounds at a load where nearly every group is full.
TEST_CASE("group_index_churn_at_full_load") {
    auto map = u64_map<ankerl::unordered_dense::bucket_type::group>();
    counter counts;
    constexpr size_t num_elements = 50000;
    map.reserve(num_elements);
    auto const buckets = map.bucket_count();
    auto rng = std::mt19937_64(31337);
    auto live = std::vector<uint64_t>();
    uint64_t next = 0;
    while (live.size() < num_elements) {
        map[next] = live.size();
        live.push_back(next);
        ++next;
    }
    REQUIRE(map.bucket_count() == buckets); // reserve was enough, no growth
    for (size_t round = 0; round < 4; ++round) {
        for (size_t i = 0; i < num_elements; ++i) {
            auto const slot = rng() % live.size();
            REQUIRE(map.erase(live[slot]) == 1U);
            REQUIRE(map.try_emplace(next, i).second);
            live[slot] = next;
            ++next;
        }
        REQUIRE(map.size() == num_elements);
        REQUIRE(map.bucket_count() == buckets); // it never grew
        for (auto k : live) {
            REQUIRE(map.contains(k));
        }
        REQUIRE(!map.contains(next));
    }
}

TEST_CASE_MAP("group_map_basics", counter::obj, counter::obj) {
    // one map_t of the four is the group index; the rest of the suite covers the others as well
    counter counts;
    INFO(counts);
    auto map = map_t();
    for (size_t i = 0; i < 1000; ++i) {
        map.try_emplace({i, counts}, i, counts);
    }
    REQUIRE(map.size() == 1000U);
    for (size_t i = 0; i < 1000; i += 2) {
        REQUIRE(map.erase({i, counts}) == 1U);
    }
    REQUIRE(map.size() == 500U);
    for (size_t i = 0; i < 1000; ++i) {
        REQUIRE(map.contains({i, counts}) == (i % 2 == 1));
    }
}
