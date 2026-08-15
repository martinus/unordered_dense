#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/hashers.h>

#include <cstddef>     // for size_t
#include <cstdint>     // for uint32_t, uint64_t
#include <functional>  // for equal_to
#include <stdexcept>   // for out_of_range
#include <string>      // for string, basic_string
#include <string_view> // for string_view, basic_string_view
#include <utility>     // for as_const

// Issue #156. Looking the same key up over and over hashes it every time, and for a long key that
// hashing is most of what a lookup costs. hash_for() computes it once, and the lookups take what it
// returns.

using namespace std::literals;

namespace {

// Long enough that hashing it is real work, which is the situation the feature is for.
auto long_key(std::string const& name) -> std::string {
    return name + std::string(200, '.');
}

// Counts hashing, so a test can show that a precomputed lookup does none. The count is global
// because the table stores the hasher by value and copies it around.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
size_t g_num_hashed = 0;

struct counting_hash {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(std::string const& str) const noexcept -> uint64_t {
        ++g_num_hashed;
        return ankerl::unordered_dense::hash<std::string>{}(str);
    }
};

using counting_map = ankerl::unordered_dense::map<std::string, int, counting_hash>;

// The two hashers whose results mixed_hash() has to finalize differently: one that says nothing
// about its quality, so wyhash is applied to it, and a 32 bit one that is avalanching, so it gets
// multiplied up into 64 bits. A precomputed hash has to survive both.
struct weak_hash {
    [[nodiscard]] auto operator()(int x) const noexcept -> uint64_t {
        return static_cast<uint64_t>(x);
    }
};

} // namespace

TEST_CASE_MAP("precomputed_hash_answers_the_same_as_hashing_again", std::string, int) {
    auto map = map_t();
    for (int i = 0; i < 100; ++i) {
        map[long_key(std::to_string(i))] = i;
    }

    for (int i = 0; i < 120; ++i) {
        auto const key = long_key(std::to_string(i));
        auto const hash = map.hash_for(key);

        REQUIRE((map.find(key, hash) == map.end()) == (map.find(key) == map.end()));
        REQUIRE(map.contains(key, hash) == map.contains(key));
        REQUIRE(map.count(key, hash) == map.count(key));
        REQUIRE(map.equal_range(key, hash) == map.equal_range(key));

        if (map.contains(key)) {
            REQUIRE(map.find(key, hash)->second == i);
            REQUIRE(map.at(key, hash) == i);
        }
    }
}

TEST_CASE_SET("precomputed_hash_answers_the_same_in_a_set", std::string) {
    auto set = set_t();
    for (int i = 0; i < 100; ++i) {
        set.insert(long_key(std::to_string(i)));
    }

    for (int i = 0; i < 120; ++i) {
        auto const key = long_key(std::to_string(i));
        auto const hash = set.hash_for(key);

        REQUIRE((set.find(key, hash) == set.end()) == (set.find(key) == set.end()));
        REQUIRE(set.contains(key, hash) == set.contains(key));
        REQUIRE(set.count(key, hash) == set.count(key));
        REQUIRE(set.equal_range(key, hash) == set.equal_range(key));
    }
}

// The point of the whole feature: the hasher does not run again.
TEST_CASE("a_precomputed_lookup_does_not_hash") {
    auto map = counting_map();
    for (int i = 0; i < 100; ++i) {
        map[long_key(std::to_string(i))] = i;
    }

    auto const present = long_key("42");
    auto const absent = long_key("nowhere");
    auto const present_hash = map.hash_for(present);
    auto const absent_hash = map.hash_for(absent);

    auto const before = g_num_hashed;
    for (int i = 0; i < 10; ++i) {
        REQUIRE(map.find(present, present_hash)->second == 42);
        REQUIRE(map.find(absent, absent_hash) == map.end());
        REQUIRE(map.contains(present, present_hash));
        REQUIRE(map.count(absent, absent_hash) == 0);
        REQUIRE(map.at(present, present_hash) == 42);
    }
    REQUIRE(g_num_hashed == before);

    // ... whereas the ordinary lookups do.
    REQUIRE(map.find(present)->second == 42);
    REQUIRE(g_num_hashed == before + 1);
}

// Which proves the other way around too: the hash handed in is the one used, so a mismatched one
// misses. This is the documented failure mode, and it has to be this and not a crash.
TEST_CASE_MAP("a_hash_of_another_key_finds_nothing", std::string, int) {
    auto map = map_t();
    for (int i = 0; i < 100; ++i) {
        map[long_key(std::to_string(i))] = i;
    }

    auto const key = long_key("42");
    auto const wrong = map.hash_for(long_key("43"));

    REQUIRE(map.find(key) != map.end()); // it is in there
    REQUIRE(map.find(key, wrong) == map.end());
    REQUIRE_FALSE(map.contains(key, wrong));
    REQUIRE(map.count(key, wrong) == 0);
    REQUIRE(map.equal_range(key, wrong) == std::pair(map.end(), map.end()));

    // NOLINTNEXTLINE(llvm-else-after-return,readability-else-after-return)
    REQUIRE_THROWS_AS(static_cast<void>(map.at(key, wrong)), std::out_of_range);
}

// A default constructed table has no buckets at all until the first insert, so a lookup that skips
// the hashing must still not go probing an empty bucket array.
TEST_CASE_MAP("an_empty_table_answers_without_probing", std::string, int) {
    auto map = map_t();
    auto const key = long_key("anything");
    auto const hash = map.hash_for(key);

    REQUIRE(map.bucket_count() == 0);
    REQUIRE(map.find(key, hash) == map.end());
    REQUIRE_FALSE(map.contains(key, hash));
    REQUIRE(map.count(key, hash) == 0);
    REQUIRE(map.equal_range(key, hash) == std::pair(map.end(), map.end()));

    // NOLINTNEXTLINE(llvm-else-after-return,readability-else-after-return)
    REQUIRE_THROWS_AS(static_cast<void>(map.at(key, hash)), std::out_of_range);

    // Emptied by erasing rather than never filled: buckets exist, values do not.
    map[key] = 1;
    map.erase(key);
    REQUIRE(map.bucket_count() != 0);
    REQUIRE(map.find(key, hash) == map.end());
}

TEST_CASE_MAP("precomputed_lookups_work_on_a_const_table", std::string, int) {
    auto map = map_t();
    map[long_key("a")] = 1;

    auto const& cmap = std::as_const(map);
    auto const hash = cmap.hash_for(long_key("a"));

    typename map_t::const_iterator const it = cmap.find(long_key("a"), hash);
    REQUIRE(it != cmap.end());
    REQUIRE(it->second == 1);
    REQUIRE(cmap.contains(long_key("a"), hash));
    REQUIRE(cmap.count(long_key("a"), hash) == 1);
    REQUIRE(cmap.equal_range(long_key("a"), hash).first == it);
    REQUIRE(cmap.at(long_key("a"), hash) == 1);
}

// The hash belongs to the hasher, not to the table it was taken from -- so a map, a set and a
// segmented map that hash the key the same way all take the same one. This is what keeps the type
// from being nested in the table: nested, it would discriminate on the mapped type, the container
// and the bucket type, none of which a hash depends on.
TEST_CASE("a_hash_carries_over_to_every_table_with_the_same_hasher") {
    auto map = ankerl::unordered_dense::map<std::string, int>();
    auto set = ankerl::unordered_dense::set<std::string>();
    auto segmented = ankerl::unordered_dense::segmented_map<std::string, long>();

    auto const key = long_key("shared");
    map[key] = 1;
    set.insert(key);
    segmented[key] = 2;

    auto const hash = map.hash_for(key);
    REQUIRE(hash.m_mixed_hash == set.hash_for(key).m_mixed_hash);
    REQUIRE(map.find(key, hash)->second == 1);
    REQUIRE(set.find(key, hash) != set.end());
    REQUIRE(segmented.find(key, hash)->second == 2);
}

// Heterogeneous lookup: hash a string_view once, look up with whatever compares equal to it.
TEST_CASE("a_precomputed_hash_works_across_key_types") {
    auto map = ankerl::unordered_dense::map<std::string, int, test::transparent_hash, std::equal_to<>>();
    map[long_key("a")] = 1;

    auto const key = long_key("a");
    auto const from_view = map.hash_for(std::string_view(key));
    auto const from_string = map.hash_for(key);

    REQUIRE(from_view.m_mixed_hash == from_string.m_mixed_hash);
    REQUIRE(map.find(std::string_view(key), from_string)->second == 1);
    REQUIRE(map.find(key, from_view)->second == 1);
    REQUIRE(map.at(std::string_view(key), from_view) == 1);
    REQUIRE(map.count(std::string_view(key), from_view) == 1);
    REQUIRE(map.contains(std::string_view(key), from_view));
    REQUIRE(map.equal_range(std::string_view(key), from_view).first->second == 1);

    // at(key, precomputed_hash) is four overloads like the rest of them, and every call above is on
    // a table the test still owns by value. The const transparent one -- a lookup key that is not
    // the key type, on a table reached through a const reference -- had nothing calling it at all:
    // its whole body could be deleted with the suite still green.
    auto const& cmap = map;
    REQUIRE(cmap.at(std::string_view(key), from_view) == 1);

    auto const absent = long_key("absent");
    auto const absent_hash = map.hash_for(std::string_view(absent));
    REQUIRE_THROWS_AS(static_cast<void>(cmap.at(std::string_view(absent), absent_hash)), std::out_of_range);
}

// equal_range and count come in four overloads apiece -- exact key or transparent, const table or
// not -- and each is a separate copy of the same two lines. The tests above reach all of them, but
// with a single element (where `it + 1` and `end()` are the same iterator) or only for a key that
// is present (where `? 0 : 1` gives the right answer whichever way round it is). This asks each of
// them the two questions that tell those apart.
TEST_CASE("precomputed_equal_range_and_count_in_every_overload") {
    auto map = ankerl::unordered_dense::map<std::string, int, test::transparent_hash, std::equal_to<>>();
    for (int i = 0; i < 100; ++i) {
        map[long_key(std::to_string(i))] = i;
    }
    auto const& cmap = std::as_const(map);

    // The key at begin(), so that "one past the hit" is as far from "the end" as it gets.
    auto const key = map.begin()->first;
    auto const view = std::string_view(key);
    auto const absent = long_key("nowhere");
    auto const absent_view = std::string_view(absent);

    auto const hash = map.hash_for(key);
    auto const absent_hash = map.hash_for(absent);

    REQUIRE(map.equal_range(key, hash) == std::pair(map.begin(), std::next(map.begin())));
    REQUIRE(cmap.equal_range(key, hash) == std::pair(cmap.begin(), std::next(cmap.begin())));
    REQUIRE(map.equal_range(view, hash) == std::pair(map.begin(), std::next(map.begin())));
    REQUIRE(cmap.equal_range(view, hash) == std::pair(cmap.begin(), std::next(cmap.begin())));

    REQUIRE(map.equal_range(absent, absent_hash) == std::pair(map.end(), map.end()));
    REQUIRE(cmap.equal_range(absent, absent_hash) == std::pair(cmap.end(), cmap.end()));
    REQUIRE(map.equal_range(absent_view, absent_hash) == std::pair(map.end(), map.end()));
    REQUIRE(cmap.equal_range(absent_view, absent_hash) == std::pair(cmap.end(), cmap.end()));

    REQUIRE(map.count(key, hash) == 1);
    REQUIRE(map.count(view, hash) == 1);
    REQUIRE(map.count(absent, absent_hash) == 0);
    REQUIRE(map.count(absent_view, absent_hash) == 0);

    REQUIRE(map.contains(key, hash));
    REQUIRE(map.contains(view, hash));
    REQUIRE(!map.contains(absent, absent_hash));
    REQUIRE(!map.contains(absent_view, absent_hash));
}

// Both finalizations mixed_hash() can apply, exercised through the public API. If hash_for() and
// the lookup ever disagreed about which one to use, these would find nothing.
TEST_CASE("finalization_matches_for_every_kind_of_hasher") {
    auto weak = ankerl::unordered_dense::map<int, int, weak_hash, std::equal_to<int>>();
    auto narrow = ankerl::unordered_dense::map<int, int, test::narrow_avalanching_hash, std::equal_to<int>>();

    for (int i = 0; i < 1000; ++i) {
        weak[i] = i;
        narrow[i] = i;
    }

    for (int i = 0; i < 1000; ++i) {
        REQUIRE(weak.find(i, weak.hash_for(i))->second == i);
        REQUIRE(narrow.find(i, narrow.hash_for(i))->second == i);
    }

    REQUIRE(weak.find(1000, weak.hash_for(1000)) == weak.end());
    REQUIRE(narrow.find(1000, narrow.hash_for(1000)) == narrow.end());
}

// Growing rehashes everything, but a key's hash does not depend on the table's size, so one taken
// before a growth stays good after it.
TEST_CASE_MAP("a_hash_survives_rehashing", std::string, int) {
    auto map = map_t();
    auto const key = long_key("kept");
    map[key] = 7;

    auto const hash = map.hash_for(key);
    for (int i = 0; i < 10000; ++i) {
        map[long_key(std::to_string(i))] = i;
    }
    REQUIRE(map.find(key, hash)->second == 7);

    map.rehash(0);
    REQUIRE(map.find(key, hash)->second == 7);

    auto moved = std::move(map);
    REQUIRE(moved.find(key, hash)->second == 7);
}
