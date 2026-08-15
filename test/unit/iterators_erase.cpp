#include <ankerl/unordered_dense.h>

#include <app/counter.h>
#include <app/doctest.h>

#include <cstddef>       // for size_t
#include <cstdint>       // for uint64_t
#include <iterator>      // for prev
#include <unordered_set> // for unordered_set
#include <utility>       // for pair
#include <vector>        // for vector

TEST_CASE_MAP("iterators_erase", counter::obj, counter::obj) {
    auto counts = counter();
    INFO(counts);
    {
        counts("begin");
        auto map = map_t();
        for (size_t i = 0; i < 100; ++i) {
            map[counter::obj(i * 101, counts)] = counter::obj(i * 101, counts);
        }

        auto it = map.find(counter::obj(size_t{20} * 101, counts));
        REQUIRE(map.size() == 100);
        REQUIRE(map.end() != map.find(counter::obj(size_t{20} * 101, counts)));
        it = map.erase(it);
        REQUIRE(map.size() == 99);
        REQUIRE(map.end() == map.find(counter::obj(size_t{20} * 101, counts)));

        it = map.begin();
        size_t current_size = map.size();
        std::unordered_set<uint64_t> keys;
        while (it != map.end()) {
            REQUIRE(keys.emplace(it->first.get()).second);
            it = map.erase(it);
            current_size--;
            REQUIRE(map.size() == current_size);
        }
        REQUIRE(map.size() == static_cast<size_t>(0));
        counts("done");
    }
    counts("destructed");
    REQUIRE(counts.dtor() ==
            counts.ctor() + counts.static_default_ctor + counts.copy_ctor() + counts.default_ctor() + counts.move_ctor());
}

// What erase() hands back. The loop above erases from begin() every time, and begin() is also what
// the erased position *is* there -- so it passes just as happily if the return value is a constant
// begin() and never the position at all. Erasing from the middle is what tells those apart, and it
// is also the only case where the returned iterator has any work to do: closing the hole moves the
// last element into the erased slot, so "the position that was erased" now holds a different
// element, and that is what makes the erase-while-iterating loop above visit everything exactly
// once.
TEST_CASE_MAP("erase_returns_the_position_it_erased", int, int) {
    auto map = map_t();
    for (int i = 0; i < 10; ++i) {
        map.try_emplace(i, i * 10);
    }

    auto const idx = typename map_t::difference_type{3};
    auto const moved_key = std::prev(map.end())->first; // the last element closes the hole
    auto const erased_key = (map.begin() + idx)->first;

    auto const it = map.erase(map.begin() + idx);
    REQUIRE(it == map.begin() + idx);
    REQUIRE(it->first == moved_key);
    REQUIRE(map.size() == 9);
    REQUIRE(map.find(erased_key) == map.end());
    REQUIRE(map.find(moved_key) == it);

    // Erasing the last element has nothing to move, and end() is the position. The erase gets a
    // statement of its own: written as `erase(...) == map.end()` the two sides are indeterminately
    // sequenced, so end() may be read before the element goes away.
    auto const past_the_last = map.erase(map.begin() + 8);
    REQUIRE(past_the_last == map.end());
    REQUIRE(map.size() == 8);
}

// erase(const_iterator) and extract(const_iterator) are one-line adaptors -- `begin() + (it -
// cbegin())` -- and nothing was calling them with an iterator that is not begin(), where the
// arithmetic cannot be wrong.
TEST_CASE_MAP("erase_and_extract_accept_a_const_iterator_from_anywhere", int, int) {
    auto map = map_t();
    for (int i = 0; i < 10; ++i) {
        map.try_emplace(i, i * 10);
    }

    auto const idx = typename map_t::difference_type{4};
    typename map_t::const_iterator const cit = map.cbegin() + idx;
    auto const erased_key = cit->first;
    auto const moved_key = std::prev(map.cend())->first;

    auto const it = map.erase(cit);
    REQUIRE(it == map.begin() + idx);
    REQUIRE(it->first == moved_key);
    REQUIRE(map.find(erased_key) == map.end());
    REQUIRE(map.size() == 9);

    typename map_t::const_iterator const cit2 = map.cbegin() + 2;
    auto const extracted = map.extract(cit2);
    REQUIRE(extracted.second == extracted.first * 10);
    REQUIRE(map.find(extracted.first) == map.end());
    REQUIRE(map.size() == 8);
}
