#include <ankerl/unordered_dense.h>
#include <fuzz/provider.h>
#include <fuzz/run.h>

#include <app/doctest.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

// The index's own structure, fuzzed where the other targets cannot reach.
//
// The map takes a key's group from the top bits of its hash and its fingerprint from the low byte,
// so with a hash the fuzzer controls, a key *names the slot it wants*. That is the difference that
// matters here. The other targets hash with wyhash or with an identity over the whole 64 bit key,
// and under either, filling one group and then emptying it again is a sequence of sixteen keys
// agreeing in their top bits followed by sixteen erases of those same keys -- structure a random
// key stream does not produce, which is why an unbounded miss survived all of them. Here the
// group, the fingerprint class and the identity of a key are three separate bytes, so the fuzzer
// reaches "fill this group, send one key of this class past it, take the fillers back out" in a
// handful of mutations, and every counter, saturation and wrap-around path with it.
//
// A wrong answer is caught against std::unordered_map. A probe that never ends is caught by
// libFuzzer's own -timeout, which is what the bug this target exists for looked like.

namespace {

// The top byte selects the group for every array size up to 256 groups, the low byte is the
// fingerprint, and the middle keeps keys distinct.
struct steerable_hash {
    using is_avalanching = void;

    auto operator()(uint64_t key) const noexcept -> uint64_t {
        return key;
    }
};

using steered_map_t = ankerl::unordered_dense::map<uint64_t, uint64_t, steerable_hash>;

auto make_key(uint8_t group, uint8_t id, uint8_t fingerprint) -> uint64_t {
    return (static_cast<uint64_t>(group) << 56U) | (static_cast<uint64_t>(id) << 8U) | fingerprint;
}

void group_index(fuzz::provider p) {
    auto map = steered_map_t();
    auto ref = std::unordered_map<uint64_t, uint64_t>();
    auto value = uint64_t{};

    // Every key ever used, so that erases and lookups can revisit one instead of always naming a
    // fresh one -- an erase of a key that was never inserted exercises nothing.
    auto seen = std::vector<uint64_t>();

    auto a_key = [&](fuzz::provider& q) {
        if (!seen.empty() && q.integral<bool>()) {
            return seen[q.bounded<size_t>(seen.size())];
        }
        auto const key = make_key(q.integral<uint8_t>(), q.integral<uint8_t>(), q.integral<uint8_t>());
        seen.push_back(key);
        return key;
    };

    while (p.has_remaining_bytes()) {
        switch (p.bounded<size_t>(8)) {
        case 0: { // insert
            auto const key = a_key(p);
            REQUIRE(map.try_emplace(key, value).second == ref.try_emplace(key, value).second);
            ++value;
            break;
        }
        case 1: { // erase by key
            auto const key = a_key(p);
            REQUIRE(map.erase(key) == ref.erase(key));
            break;
        }
        case 2: { // erase by iterator, which finds the slot from the value rather than by probing
            auto const key = a_key(p);
            auto it = map.find(key);
            if (it != map.end()) {
                map.erase(it);
                ref.erase(key);
            }
            break;
        }
        case 3: { // lookup, the operation that has to terminate whatever the counters say
            auto const key = a_key(p);
            REQUIRE(map.contains(key) == (ref.count(key) != 0));
            break;
        }
        case 4: { // fill a whole group and overflow it: the shape that leaves a counter behind
            auto const group = p.integral<uint8_t>();
            auto const fingerprint = p.integral<uint8_t>();
            auto const count = p.range<uint8_t>(1, 20);
            for (uint8_t id = 0; id < count; ++id) {
                auto const key = make_key(group, id, fingerprint);
                seen.push_back(key);
                map.try_emplace(key, value);
                ref.try_emplace(key, value);
                ++value;
            }
            break;
        }
        case 5: { // and take a run of them back out, which is what leaves the counter without its
                  // entry's neighbours
            auto const group = p.integral<uint8_t>();
            auto const fingerprint = p.integral<uint8_t>();
            auto const count = p.range<uint8_t>(1, 20);
            for (uint8_t id = 0; id < count; ++id) {
                auto const key = make_key(group, id, fingerprint);
                REQUIRE(map.erase(key) == ref.erase(key));
            }
            break;
        }
        case 6: { // rehash rebuilds the index from the values, which is the one thing that repairs it
            map.rehash(p.bounded<size_t>(1024));
            break;
        }
        default: {
            if (p.integral<bool>()) {
                map.reserve(p.bounded<size_t>(1024));
            } else {
                auto copy = map;
                map = std::move(copy);
            }
            break;
        }
        }
        REQUIRE(map.size() == ref.size());
    }

    for (auto const& [key, val] : ref) {
        auto it = map.find(key);
        REQUIRE(it != map.end());
        REQUIRE(it->second == val);
    }
    for (auto const& [key, val] : map) {
        auto it = ref.find(key);
        REQUIRE(it != ref.end());
        REQUIRE(it->second == val);
    }
}

} // namespace

FUZZ_TEST_CASE(fuzz_group_index, p) {
    group_index(p.copy());
}
