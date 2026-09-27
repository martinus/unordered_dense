#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstdint> // for uint64_t
#include <utility> // for move

// A table without a bucket array of its own reads a shared, never written sentinel index, so that a
// lookup needs no test for the empty table (#329). What that has to keep true: lookups in such a
// table find nothing, the first insert allocates before it writes, nothing ever writes into the
// sentinel -- every empty table in the process would see it -- and a moved-from table does not
// keep pointing at the index it gave away.

TEST_CASE_MAP("sentinel_lookups_in_an_empty_table_find_nothing", std::uint64_t, std::uint64_t) {
    auto m = map_t();
    for (std::uint64_t k = 0; k < 1000; ++k) {
        REQUIRE(m.find(k) == m.end());
        REQUIRE_FALSE(m.contains(k));
        REQUIRE(m.count(k) == 0);
        REQUIRE(m.erase(k) == 0);
    }
    REQUIRE(m.bucket_count() == 0);
    REQUIRE(m.try_emplace(7, 8).second);
    REQUIRE(m.bucket_count() > 0);
    REQUIRE(m.find(7)->second == 8);
}

TEST_CASE_MAP("sentinel_is_never_written", std::uint64_t, std::uint64_t) {
    // Every way to arrive at a table without an array of its own, then lookups, erases and a first
    // insert in it; then a fresh empty table must still find nothing for any key. The first insert
    // may only write once it has grown, which it does because such a table has a capacity of zero.
    for (std::uint64_t round = 0; round < 50; ++round) {
        auto filled = map_t();
        for (std::uint64_t k = 0; k < 100; ++k) {
            filled[k + round] = k;
        }
        auto const empty = map_t();
        auto assigned = filled;
        assigned = empty; // copy assignment from an empty table
        auto moved_from = filled;
        auto moved_to = std::move(moved_from);
        auto default_constructed = map_t();
        // NOLINTNEXTLINE(bugprone-use-after-move,hicpp-invalid-access-moved)
        for (auto* m : {&assigned, &moved_from, &default_constructed}) {
            for (std::uint64_t k = 0; k < 200; ++k) {
                REQUIRE(m->find(k * 7919 + round) == m->end());
                REQUIRE(m->erase(k * 7919 + round) == 0);
            }
            (*m)[round] = round;
            REQUIRE(m->size() == 1);
        }
        REQUIRE(moved_to.size() == 100);
    }
    auto const fresh = map_t();
    for (std::uint64_t k = 0; k < 20000; ++k) {
        REQUIRE(fresh.find(k) == fresh.end());
    }
}

TEST_CASE_MAP("sentinel_moved_from_table_does_not_keep_the_index", std::uint64_t, std::uint64_t) {
    auto a = map_t();
    for (std::uint64_t k = 0; k < 100; ++k) {
        a[k] = k;
    }
    auto b = std::move(a);
    REQUIRE(b.size() == 100);
    // NOLINTBEGIN(bugprone-use-after-move,hicpp-invalid-access-moved)
    a.clear();
    for (std::uint64_t k = 0; k < 100; ++k) {
        REQUIRE(a.find(k) == a.end());
    }
    a[1000] = 1;
    REQUIRE(a.size() == 1);
    REQUIRE(a.find(1000)->second == 1);
    // NOLINTEND(bugprone-use-after-move,hicpp-invalid-access-moved)

    auto c = map_t();
    c = std::move(b);
    REQUIRE(c.size() == 100);
    // NOLINTBEGIN(bugprone-use-after-move,hicpp-invalid-access-moved)
    b.clear();
    for (std::uint64_t k = 0; k < 100; ++k) {
        REQUIRE(b.find(k) == b.end());
    }
    // NOLINTEND(bugprone-use-after-move,hicpp-invalid-access-moved)

    auto d = map_t();
    auto const empty = map_t();
    d = empty;
    for (std::uint64_t k = 0; k < 100; ++k) {
        REQUIRE(d.find(k) == d.end());
    }
    d[3] = 4;
    REQUIRE(d.find(3)->second == 4);
}
