#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstddef> // for size_t
#include <cstdint> // for uint64_t
#include <vector>  // for vector

// A mutating hit on an entry sitting past its home group moves it home when there is room. This
// churns like erase_churn does, at a fixed size with the index never rebuilt, and touches a live
// key through operator[] every cycle so that entries keep moving; then asks for every live key
// and its value, which is what a slot that moved to the wrong lane, kept a stale copy behind, or
// took a counter down that it never put up would break.
TEST_CASE("move_home_keeps_every_live_key_and_value") {
    using map_t = ankerl::unordered_dense::map<uint64_t, uint64_t>;

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
    std::vector<uint64_t> bumps; // how often operator[] has touched live[i]
    while (live.size() < 3250) { // load 0.79 of the 0.8 maximum
        auto const key = next();
        if (map.try_emplace(key, ~key).second) {
            live.push_back(key);
            bumps.push_back(0);
        }
    }

    size_t constexpr cycles = 200000;
    size_t constexpr check_every = 8192;
    for (size_t cycle = 0; cycle < cycles; ++cycle) {
        auto const victim = static_cast<size_t>(next() % live.size());
        auto const gone = live[victim];
        REQUIRE(map.erase(gone) == 1U);
        live[victim] = live.back();
        bumps[victim] = bumps.back();
        live.pop_back();
        bumps.pop_back();

        auto const touched = static_cast<size_t>(next() % live.size());
        map[live[touched]] += 1;
        ++bumps[touched];

        auto const fresh = next();
        REQUIRE(map.try_emplace(fresh, ~fresh).second);
        live.push_back(fresh);
        bumps.push_back(0);

        if (cycle % check_every == check_every - 1) {
            INFO("after ", cycle + 1, " cycles");
            REQUIRE(map.size() == live.size());
            REQUIRE(map.bucket_count() == buckets);
            for (size_t i = 0; i < live.size(); ++i) {
                auto it = map.find(live[i]);
                REQUIRE(it != map.end());
                REQUIRE(it->second == ~live[i] + bumps[i]);
            }
            REQUIRE(map.find(gone) == map.end());
        }
    }
    for (auto const key : live) {
        REQUIRE(map.erase(key) == 1U);
    }
    REQUIRE(map.empty());
}

// The same, steered: with the identity hash the group is the top bits of the key and the
// fingerprint the low byte, so a group can be filled on purpose, one more key sent past it, the
// room made, and the move provoked -- and then the key must still be there once everything that
// was in its way is gone.
namespace {
struct identity {
    using is_avalanching = void;
    auto operator()(uint64_t k) const -> uint64_t {
        return k;
    }
};
} // namespace

TEST_CASE("move_home_after_the_home_group_empties") {
    using map_t = ankerl::unordered_dense::map<uint64_t, uint64_t, identity>;

    map_t map;
    map.reserve(1000); // 2048 slots, 128 groups: the group is key >> 57
    auto const buckets = map.bucket_count();
    REQUIRE(buckets == 2048U);
    auto const in_group = [](uint64_t group, uint64_t low) {
        return (group << 57U) | low;
    };

    // sixteen fillers fill group 5; the seventeenth goes past it
    for (uint64_t i = 0; i < 16; ++i) {
        map.try_emplace(in_group(5, 0x100 + i), i);
    }
    auto const displaced = in_group(5, 0x77);
    map.try_emplace(displaced, 99);
    REQUIRE(map.size() == 17);

    // room at home, then a mutating hit to move it, then everything else out of the way
    REQUIRE(map.erase(in_group(5, 0x100)) == 1U);
    map[displaced] += 1;
    REQUIRE(map.find(displaced)->second == 100);
    for (uint64_t i = 1; i < 16; ++i) {
        REQUIRE(map.erase(in_group(5, 0x100 + i)) == 1U);
    }
    REQUIRE(map.size() == 1);
    REQUIRE(map.find(displaced)->second == 100);
    REQUIRE(map.bucket_count() == buckets);

    // and it can be erased and reinserted like any other key
    REQUIRE(map.erase(displaced) == 1U);
    REQUIRE(map.empty());
    REQUIRE(map.try_emplace(displaced, 1).second);
    REQUIRE(map.contains(displaced));
}
