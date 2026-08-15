#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <iterator> // for next
#include <utility>  // for pair

// An iterator converts to a const_iterator, and the header spells that out twice: once as a
// converting constructor and once as a converting *assignment*, each copying the two members by
// hand. Constructing one is common enough that the tests do it all over the place -- every
// `const_iterator it = map.begin()` is that constructor. Assigning to one that already exists is
// not, and a deletion sweep found the assignment's whole body removable with nothing going red.
//
// The distinction matters because the two are separately written. An assignment operator that
// copies only one of the two members, or returns without copying either, leaves an iterator that
// still points at the container it did before -- which is exactly the loop-variable pattern below.

TEST_CASE_MAP("assigning_an_iterator_to_an_existing_const_iterator", int, int) {
    auto map = map_t();
    for (int i = 0; i < 10; ++i) {
        map.try_emplace(i, i * 10);
    }

    // default-adjacent: a const_iterator that already holds something else, so the assignment has
    // to overwrite both members rather than merely being a disguised construction.
    typename map_t::const_iterator cit = map.cend();
    REQUIRE(cit == map.cend());

    cit = map.begin();
    REQUIRE(cit == map.cbegin());
    REQUIRE(cit != map.cend());
    REQUIRE(cit->second == cit->first * 10);

    // and again to a different position, which is what catches an operator= that copies the data
    // pointer but not the index
    cit = std::next(map.begin(), 4);
    REQUIRE(cit == std::next(map.cbegin(), 4));
    REQUIRE(cit->second == cit->first * 10);
    REQUIRE(std::next(cit) != map.cend());

    // walking to the end through the assigned-to iterator has to arrive exactly at end()
    auto count = 0;
    for (cit = map.begin(); cit != map.cend(); ++cit) {
        ++count;
    }
    REQUIRE(count == 10);

    // The iterator holds two members: which container, and where in it. Every assignment above is
    // between two iterators into the *same* map, so they already agree about the container and an
    // operator= that forgot to copy that half would still pass all of it. Assigning from an
    // iterator into a different container is the only thing that can tell.
    auto other = map_t();
    other.try_emplace(99, 990);

    cit = other.begin();
    REQUIRE(cit->first == 99);
    REQUIRE(cit->second == 990);
}
