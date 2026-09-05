#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>

// The index: sixteen fingerprints and eight overflow counters per group, value indices beside
// them. The whole suite runs on it, since it is the only index there is; what is here is what
// only makes sense to ask of it directly.

using map_t = ankerl::unordered_dense::map<std::string, size_t>;

// max_size() is a multiple of sixteen, so the largest array is whole groups
static_assert(map_t::max_bucket_count() % 16U == 0U);

TEST_CASE("group_bucket_count_is_in_slots") {
    auto map = map_t();
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
