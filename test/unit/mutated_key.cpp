#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstdint>
#include <stdexcept>
#include <string>

// This map hands out a mutable key -- README calls "no `const Key` in `std::pair<Key, Value>`" a
// disadvantage, and `replace_key()` exists so that changing one is a supported operation. What is not
// supported is changing it behind the map's back and then asking the map to find the element again:
// every erase-shaped path looks the element up by hashing the key it can see, and after an assignment
// that hash names a probe sequence the element is not on.
//
// Until 2026-09-11 what happened then was that `slot_of_value` walked that sequence for ever at 100%
// of a core (#254). It is bounded now, so the outcome is diagnosable. The tests below are about the
// termination, not about the exception type: what they would catch is the bound coming off again.

#if ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS()

namespace {

auto mutated_key_map() -> ankerl::unordered_dense::map<std::string, int> {
    auto map = ankerl::unordered_dense::map<std::string, int>();
    for (int i = 0; i < 200; ++i) {
        map.try_emplace("key " + std::to_string(i), i);
    }
    return map;
}

} // namespace

TEST_CASE("mutated_key_erase_by_iterator_terminates") {
    auto map = mutated_key_map();
    auto it = map.find("key 42");
    REQUIRE(it != map.end());
    it->first = "a key that hashes somewhere else entirely";
    REQUIRE_THROWS_AS(map.erase(it), std::logic_error);
}

TEST_CASE("mutated_key_extract_by_iterator_terminates") {
    auto map = mutated_key_map();
    auto it = map.find("key 7");
    REQUIRE(it != map.end());
    it->first = "a key that hashes somewhere else entirely";
    REQUIRE_THROWS_AS(map.extract(it), std::logic_error);
}

TEST_CASE("mutated_key_replace_key_terminates") {
    auto map = mutated_key_map();
    auto it = map.find("key 99");
    REQUIRE(it != map.end());
    it->first = "a key that hashes somewhere else entirely";
    REQUIRE_THROWS_AS(map.replace_key(it, "something new"), std::logic_error);
}

// The other way in: the element whose key was changed is not the one being erased, it is the one the
// backfill drags into the hole. finish_erase() looks *that* one up, so the throw comes from a call
// the caller never made against an element the caller did not name.
TEST_CASE("mutated_key_found_by_the_backfill_terminates") {
    auto map = mutated_key_map();
    auto last = map.end() - 1;
    last->first = "a key that hashes somewhere else entirely";
    auto victim = map.begin();
    REQUIRE(victim != last);
    REQUIRE_THROWS_AS(map.erase(victim), std::logic_error);
}

// A table of exactly one group, where the bound is `delta == 0` and fires on the first pass: the
// smallest index is four groups today, so this is the shape the bound's own edge takes.
TEST_CASE("mutated_key_in_a_tiny_table_terminates") {
    auto map = ankerl::unordered_dense::map<uint64_t, int>();
    for (uint64_t i = 0; i < 3; ++i) {
        map.try_emplace(i * UINT64_C(0x9E3779B97F4A7C15), static_cast<int>(i));
    }
    auto it = map.begin();
    it->first = ~it->first;
    REQUIRE_THROWS_AS(map.erase(it), std::logic_error);
}

// And the bound must not fire on anything legitimate: a table churned until elements are sitting well
// past their home groups, then emptied one element at a time.
TEST_CASE("mutated_key_bound_never_fires_on_a_churned_table") {
    auto map = ankerl::unordered_dense::map<uint64_t, uint64_t>();
    auto state = UINT64_C(0x9E3779B97F4A7C15);
    auto next = [&state] {
        state += UINT64_C(0x9E3779B97F4A7C15);
        auto z = state;
        z = (z ^ (z >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27U)) * UINT64_C(0x94D049BB133111EB);
        return z ^ (z >> 31U);
    };
    map.reserve(1000);
    while (map.size() < 1000) {
        auto const key = next();
        map.try_emplace(key, ~key);
    }
    for (size_t round = 0; round < 20000; ++round) {
        auto const gone = map.begin()->first;
        REQUIRE(map.erase(gone) == 1U);
        auto const fresh = next();
        map.try_emplace(fresh, ~fresh);
    }
    while (!map.empty()) {
        map.erase(map.begin());
    }
    REQUIRE(map.empty());
}

#endif
