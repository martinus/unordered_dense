#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstddef> // for size_t
#include <cstdint> // for uint64_t
#include <vector>  // for vector

// Insert and erase at a fixed size, for a long time, with the index never rebuilt.
//
// Every other churn test grows its map, and growth rebuilds the index from the values, so a
// counter an erase left one too high or took down one too many does not live long there. Here
// nothing rebuilds it. A counter that is too high only sends a probe one group further, which no
// answer shows; one that is too low stops a probe short of an entry that overflowed past that
// group, and that entry cannot be found -- and at a load near the maximum nearly every group is
// full, so nearly every insert passes through a counter on its way to a slot.
//
// So: reserve, so the bucket count is fixed and asserted to stay so; fill to just under the
// maximum load; then churn, and every so often ask for every live key.
TEST_CASE("erase_churn_keeps_every_live_key_findable") {
    using map_t = ankerl::unordered_dense::map<uint64_t, uint64_t>;

    // splitmix64: random 64 bit keys, so the groups fill unevenly and entries overflow
    uint64_t state = 0x9E3779B97F4A7C15;
    auto next = [&state] {
        state += 0x9E3779B97F4A7C15;
        auto z = state;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EB;
        return z ^ (z >> 31U);
    };

    map_t map;
    map.reserve(3200);
    auto const buckets = map.bucket_count();
    REQUIRE(buckets == 4096U);

    std::vector<uint64_t> live;
    while (live.size() < 3250) { // load 0.79 of the 0.8 maximum
        auto const key = next();
        if (map.try_emplace(key, ~key).second) {
            live.push_back(key);
        }
    }
    REQUIRE(map.bucket_count() == buckets);

    size_t constexpr cycles = 200000;
    size_t constexpr check_every = 8192;
    for (size_t cycle = 0; cycle < cycles; ++cycle) {
        // erase one live key at random, then insert a new one, so size and load stay put
        auto const victim = static_cast<size_t>(next() % live.size());
        auto const gone = live[victim];
        REQUIRE(map.erase(gone) == 1U);
        live[victim] = live.back();
        live.pop_back();

        auto const fresh = next();
        REQUIRE(map.try_emplace(fresh, ~fresh).second);
        live.push_back(fresh);

        if (cycle % check_every == check_every - 1) {
            INFO("after ", cycle + 1, " cycles");
            REQUIRE(map.size() == live.size());
            REQUIRE(map.bucket_count() == buckets); // no growth, so nothing has rebuilt the index
            for (auto const key : live) {
                auto it = map.find(key);
                REQUIRE(it != map.end());
                REQUIRE(it->second == ~key);
            }
            REQUIRE(map.find(gone) == map.end());
        }
    }
}
