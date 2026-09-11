#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstddef> // for size_t
#include <cstdint> // for uint64_t
#include <stdexcept>
#include <string>

// This map hands out a mutable key -- README calls "no `const Key` in `std::pair<Key, Value>`" a
// disadvantage, and `replace_key()` exists so that changing one is a supported operation. What is not
// supported is changing it behind the map's back and then asking the map to find the element again:
// every erase-shaped path looks the element up by hashing the key it can see, and after an assignment
// that hash names a probe sequence the element is not on.
//
// Until 2026-09-11 what happened then was that `slot_of_value` walked that sequence for ever at 100%
// of a core (#254). It is bounded now, so the outcome is diagnosable, and these pin both halves: that
// it comes back at all, and that it comes back as `std::logic_error` rather than by some other route.
//
// Over all four container shapes, which is not decoration: `m_group_mask` -- the right-hand side of
// the new bound -- is `value_idx_type`, a `std::uint32_t` for the default group and a `std::size_t`
// for `big_map`'s `group_big`, so only `TEST_CASE_MAP` exercises the bound at both widths.

TEST_CASE_MAP("mutated_key_terminates_from_every_entry_point", std::string, size_t) {
    auto map = map_t();
    for (size_t i = 0; i < 200; ++i) {
        map.try_emplace("key " + std::to_string(i), i);
    }
    auto it = map.find("key 42");
    REQUIRE(it != map.end());
    it->first = "a key that hashes somewhere else entirely";

    // One clobbered key, every path that then looks it up again. doctest re-runs the body per
    // subcase, so each gets its own map.
    SUBCASE("erase") {
        REQUIRE_THROWS_AS(map.erase(it), std::logic_error);
    }
    SUBCASE("extract") {
        REQUIRE_THROWS_AS(map.extract(it), std::logic_error);
    }
    SUBCASE("replace_key") {
        REQUIRE_THROWS_AS(map.replace_key(it, "something new"), std::logic_error);
    }
}

// The other way in: the element whose key was changed is not the one being erased, it is the one the
// backfill drags into the hole. finish_erase() looks *that* one up -- through repoint_value(), which
// walks the same probe sequence under the same bound -- so the throw comes from a call the caller
// never made against an element the caller did not name.
TEST_CASE_MAP("mutated_key_found_by_the_backfill_terminates", std::string, size_t) {
    auto map = map_t();
    for (size_t i = 0; i < 200; ++i) {
        map.try_emplace("key " + std::to_string(i), i);
    }
    auto last = map.end() - 1;
    last->first = "a key that hashes somewhere else entirely";
    auto victim = map.begin();
    REQUIRE(victim != last);
    REQUIRE_THROWS_AS(map.erase(victim), std::logic_error);
}

// The smallest index there is: `initial_shifts` is 64 - 2, so four groups, and the bound fires after
// four passes rather than after a long walk. It never reaches `delta == 0`, which is the edge the
// miss probe's own comment says is unreachable for the same reason.
TEST_CASE_MAP("mutated_key_in_the_smallest_table_terminates", uint64_t, int) {
    auto map = map_t();
    for (uint64_t i = 0; i < 3; ++i) {
        map.try_emplace(i * UINT64_C(0x9E3779B97F4A7C15), static_cast<int>(i));
    }
    REQUIRE(map.bucket_count() == 64U); // four groups of sixteen
    auto it = map.begin();
    it->first = ~it->first;
    REQUIRE_THROWS_AS(map.erase(it), std::logic_error);
}
