#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstdint>
#include <iterator>
#include <list>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// insert(first, last) hashes ahead of where it places, which is a different code path from
// insert(value) in a loop -- and only for an iterator that can be walked twice. What is testable is
// that the two paths cannot be told apart: same elements, same "first one wins" on duplicates, same
// answer at the range lengths where the sixteen-deep lookahead has edges.

TEST_CASE("range_insert_matches_a_loop_of_single_inserts") {
    auto input = std::vector<std::pair<uint64_t, size_t>>();
    for (size_t i = 0; i < 5000; ++i) {
        input.emplace_back(static_cast<uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15), i);
    }

    // Every length from empty to past two lookahead windows, then the whole thing: the priming
    // loop, the steady state and the tail are all short-range cases.
    for (size_t len = 0; len <= 40; ++len) {
        auto ranged = ankerl::unordered_dense::map<uint64_t, size_t>();
        auto looped = ankerl::unordered_dense::map<uint64_t, size_t>();
        ranged.insert(input.begin(), input.begin() + static_cast<std::ptrdiff_t>(len));
        for (size_t i = 0; i < len; ++i) {
            looped.insert(input[i]);
        }
        REQUIRE(ranged.size() == looped.size());
        for (auto const& [k, v] : looped) {
            REQUIRE(ranged.at(k) == v);
        }
    }

    auto ranged = ankerl::unordered_dense::map<uint64_t, size_t>();
    ranged.insert(input.begin(), input.end());
    REQUIRE(ranged.size() == input.size());
    for (auto const& [k, v] : input) {
        REQUIRE(ranged.at(k) == v);
    }
}

// insert() keeps the element already there, so a duplicate later in the range must not overwrite an
// earlier one -- the lookahead sees later keys before the earlier ones are placed, which is exactly
// where that could go wrong.
TEST_CASE("range_insert_keeps_the_first_of_each_duplicate") {
    auto input = std::vector<std::pair<int, int>>();
    for (int round = 0; round < 40; ++round) {
        for (int i = 0; i < 20; ++i) {
            input.emplace_back(i, round); // the same twenty keys, forty times, spanning the window
        }
    }
    auto map = ankerl::unordered_dense::map<int, int>();
    map.insert(input.begin(), input.end());
    REQUIRE(map.size() == 20U);
    for (int i = 0; i < 20; ++i) {
        REQUIRE(map.at(i) == 0); // the first round's value, not the last
    }
}

// Into a map that already holds things, and one that has never allocated buckets.
TEST_CASE("range_insert_into_a_populated_and_an_empty_map") {
    auto input = std::vector<std::pair<int, int>>();
    for (int i = 0; i < 1000; ++i) {
        input.emplace_back(i, i * 2);
    }
    auto fresh = ankerl::unordered_dense::map<int, int>();
    REQUIRE(fresh.bucket_count() == 0U);
    fresh.insert(input.begin(), input.end());
    REQUIRE(fresh.size() == 1000U);

    auto populated = ankerl::unordered_dense::map<int, int>();
    for (int i = 500; i < 1500; ++i) {
        populated[i] = -1;
    }
    populated.insert(input.begin(), input.end());
    REQUIRE(populated.size() == 1500U);
    REQUIRE(populated.at(0) == 0);     // new, from the range
    REQUIRE(populated.at(999) == -1);  // already there, so the range did not overwrite it
    REQUIRE(populated.at(1499) == -1); // untouched
}

// A single-pass iterator cannot be walked twice, so it takes the plain loop rather than the
// lookahead. It has to keep working, which is the only thing that says the fallback is wired up.
TEST_CASE("range_insert_from_an_input_iterator") {
    auto text = std::string("7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25");
    auto in = std::istringstream(text);
    auto set = ankerl::unordered_dense::set<int>();
    set.insert(std::istream_iterator<int>(in), std::istream_iterator<int>());
    REQUIRE(set.size() == 19U);
    REQUIRE(set.count(7) == 1U);
    REQUIRE(set.count(25) == 1U);
    REQUIRE(set.count(26) == 0U);
    static_assert(!ankerl::unordered_dense::detail::is_forward_iterator_v<std::istream_iterator<int>>,
                  "an istream_iterator is single-pass and must take the fallback");
    static_assert(ankerl::unordered_dense::detail::is_forward_iterator_v<std::vector<int>::iterator>,
                  "a vector iterator can be walked twice");
    static_assert(ankerl::unordered_dense::detail::is_forward_iterator_v<std::list<int>::iterator>,
                  "so can a list iterator, which is not random access");
}

// A non-contiguous forward range still takes the lookahead path.
TEST_CASE("range_insert_from_a_list") {
    auto values = std::list<std::pair<int, int>>();
    for (int i = 0; i < 500; ++i) {
        values.emplace_back(i, i);
    }
    auto map = ankerl::unordered_dense::map<int, int>();
    map.insert(values.begin(), values.end());
    REQUIRE(map.size() == 500U);
    REQUIRE(map.at(499) == 499);
}

// Against a reference, over a range big enough to grow the table several times -- growth moves the
// bucket array, which is what the prefetches in flight are aimed at.
TEST_CASE("range_insert_across_several_growths") {
    auto input = std::vector<std::pair<uint64_t, uint64_t>>();
    auto ref = std::unordered_map<uint64_t, uint64_t>();
    for (uint64_t i = 0; i < 40000; ++i) {
        auto const k = i * UINT64_C(0x9E3779B97F4A7C15);
        input.emplace_back(k, i);
        ref.emplace(k, i);
    }
    auto map = ankerl::unordered_dense::map<uint64_t, uint64_t>();
    map.insert(input.begin(), input.end());
    REQUIRE(map.size() == ref.size());
    for (auto const& [k, v] : ref) {
        REQUIRE(map.at(k) == v);
    }
    for (uint64_t i = 0; i < 1000; ++i) {
        REQUIRE(map.count(i * UINT64_C(0x9E3779B97F4A7C15) + 1) == ref.count(i * UINT64_C(0x9E3779B97F4A7C15) + 1));
    }
}

// A default-constructed table has no bucket array, and inserting nothing must not give it one --
// the same promise lazy_bucket_allocation.cpp makes of the other entry points. The range insert
// allocates before it hashes anything, so the empty case has to return before that.
TEST_CASE("range_insert_of_an_empty_range_allocates_nothing") {
    auto input = std::vector<std::pair<int, int>>();
    auto map = ankerl::unordered_dense::map<int, int>();
    map.insert(input.begin(), input.end());
    REQUIRE(map.bucket_count() == 0U);
    REQUIRE(map.empty());

    // and the same for a non-empty container's empty sub-range
    input.emplace_back(1, 1);
    map.insert(input.begin(), input.begin());
    REQUIRE(map.bucket_count() == 0U);
    map.insert(input.begin(), input.end());
    REQUIRE(map.bucket_count() != 0U);
}

TEST_CASE("range_insert_initializer_list_and_self_range") {
    auto map = ankerl::unordered_dense::map<int, int>{{1, 10}, {2, 20}, {3, 30}};
    REQUIRE(map.size() == 3U);
    REQUIRE(map.at(2) == 20);

    // inserting a map's own values back into it changes nothing
    auto copy = map;
    map.insert(copy.begin(), copy.end());
    REQUIRE(map.size() == 3U);
    REQUIRE(map.at(3) == 30);
}
