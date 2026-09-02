#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstddef> // for size_t
#include <cstdint> // for uint64_t
#include <vector>  // for vector

// Insert and erase at a fixed size, for a long time, with the bucket array never rebuilt.
//
// Every other churn test grows its map, and growth rebuilds every bucket from the values, so a
// bucket the shift loops leave behind -- pointing at a value that has moved, or neither empty nor
// at a valid distance -- does not live long there. Here nothing rebuilds it. Such a bucket is
// invisible to a find, which compares the key it reaches, and to iteration, which walks the
// values; it shows when an erase searching for the bucket of the value it is moving reaches a
// stale one first and repoints that, leaving a live value no bucket finds, or when no bucket is
// empty and an insert cannot end.
//
// A mutation sweep of the vector shifts had already caught every mutant that does this, most of
// them as a hang of the whole suite. Under this test the same mutants fail within 300 cycles,
// by name, which is what a person fixing one needs. The sixteen that survived it as well are
// equivalent: the sentinel guards, and table lanes the blend never reads.
//
// So: reserve, so the bucket count is fixed and asserted to stay so; fill to a load that shifts
// often; then churn, and every so often ask for every live key.
TEST_CASE("erase_churn_keeps_every_live_key_findable") {
    using map_t = ankerl::unordered_dense::map<uint64_t, uint64_t>;

    // splitmix64: random 64 bit keys, so the hash has real chains to shift
    uint64_t state = 0x9E3779B97F4A7C15;
    auto next = [&state] {
        state += 0x9E3779B97F4A7C15;
        auto z = state;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EB;
        return z ^ (z >> 31U);
    };

    map_t map;
    map.reserve(2800);
    auto const buckets = map.bucket_count();
    REQUIRE(buckets == 4096U);

    std::vector<uint64_t> live;
    while (live.size() < 2900) {
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
            REQUIRE(map.bucket_count() == buckets); // no growth, so nothing has cleaned the buckets
            for (auto const key : live) {
                auto it = map.find(key);
                REQUIRE(it != map.end());
                REQUIRE(it->second == ~key);
            }
            REQUIRE(map.find(gone) == map.end());
        }
    }
}
