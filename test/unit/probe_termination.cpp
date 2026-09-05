#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/hashers.h>

#include <cstdint>

// A miss stops at the first group whose counter for the key's class is zero, and the counters are
// exact -- but exact about a different question. A counter counts the live entries that overflowed
// past its group on *their* sequences, so a caller who chooses the hash can leave every group's
// counter positive with a handful of keys, and then no sequence has a zero on it. Both cases here
// build that with a hash that returns the key, and ask that a lookup still comes back.

namespace {

using steered_map_t = ankerl::unordered_dense::map<uint64_t, uint64_t, test::identity_hash>;

struct steered_key_maker {
    unsigned shift;

    // home is the group, the low byte the fingerprint; the class is fingerprint & 7
    auto operator()(uint64_t home, uint64_t id, uint64_t fingerprint) const -> uint64_t {
        return (home << shift) | (id << 8U) | fingerprint;
    }
};

auto steered_keys(steered_map_t const& map) -> steered_key_maker {
    auto shift = unsigned{64};
    for (auto g = map.bucket_count() / 16; g > 1; g /= 2) {
        --shift;
    }
    return steered_key_maker{shift};
}

} // namespace

// For each group in turn: fill it, send one more key of class 1 past it, erase the fillers. The
// passer stays, so the counter it incremented stays. Eight live keys, every class-1 counter
// positive, and a miss for class 1 has nothing to stop at but the end of the array.
TEST_CASE("probe_terminates_with_every_counter_positive") {
    auto map = steered_map_t();
    map.reserve(100);
    auto const key = steered_keys(map);
    auto const groups = map.bucket_count() / 16;
    for (uint64_t g = 0; g < groups; ++g) {
        for (uint64_t id = 0; id < 16; ++id) {
            map[key(g, id, 0x22)] = id;
        }
        map[key(g, 100, 0x11)] = g;
        for (uint64_t id = 0; id < 16; ++id) {
            REQUIRE(map.erase(key(g, id, 0x22)) == 1U);
        }
    }
    REQUIRE(map.size() == groups);

    for (uint64_t g = 0; g < groups; ++g) {
        REQUIRE(map.find(key(g, 100, 0x11))->second == g);
        REQUIRE(!map.contains(key(g, 999, 0x11)));
        REQUIRE(map.count(key(g, 999, 0x11)) == 0U);
        REQUIRE(map.erase(key(g, 999, 0x11)) == 0U);
        REQUIRE(map.try_emplace(key(g, 999, 0x11), 0).second);
        REQUIRE(map.erase(key(g, 999, 0x11)) == 1U);
    }

    // erasing the passers takes the counters down, and a miss stops in its home group again
    for (uint64_t g = 0; g < groups; ++g) {
        REQUIRE(map.erase(key(g, 100, 0x11)) == 1U);
        REQUIRE(!map.contains(key(g, 999, 0x11)));
    }
    REQUIRE(map.empty());
}

// A counter that reaches 255 stays there for the life of the array. Three hundred keys of one
// class sent past one full group saturate it and the groups after it; erasing all of them leaves
// the counters where they are and the table empty. Lookups of that class then walk further than
// they need to, and must still come back.
TEST_CASE("probe_terminates_past_a_saturated_counter") {
    auto map = steered_map_t();
    map.reserve(1000);
    auto const key = steered_keys(map);
    auto const buckets = map.bucket_count();
    constexpr uint64_t home = 5;
    constexpr uint64_t passers = 300;
    for (uint64_t id = 0; id < 16; ++id) {
        map[key(home, id, 0x22)] = id;
    }
    for (uint64_t id = 0; id < passers; ++id) {
        map[key(home, id, 0x11)] = id;
    }
    REQUIRE(map.bucket_count() == buckets);
    for (uint64_t id = 0; id < passers; ++id) {
        REQUIRE(map.find(key(home, id, 0x11))->second == id);
    }
    for (uint64_t id = 0; id < passers; ++id) {
        REQUIRE(map.erase(key(home, id, 0x11)) == 1U);
    }
    for (uint64_t id = 0; id < 16; ++id) {
        REQUIRE(map.erase(key(home, id, 0x22)) == 1U);
    }
    REQUIRE(map.empty());
    REQUIRE(map.bucket_count() == buckets);

    REQUIRE(!map.contains(key(home, 999, 0x11)));
    for (uint64_t id = 0; id < 40; ++id) {
        REQUIRE(map.try_emplace(key(home, id, 0x11), id).second);
    }
    for (uint64_t id = 0; id < 40; ++id) {
        REQUIRE(map.find(key(home, id, 0x11))->second == id);
        REQUIRE(!map.contains(key(home, id + 1000, 0x11)));
    }
    for (uint64_t id = 0; id < 40; ++id) {
        REQUIRE(map.erase(key(home, id, 0x11)) == 1U);
    }
    REQUIRE(!map.contains(key(home, 999, 0x11)));
    REQUIRE(map.empty());
}
