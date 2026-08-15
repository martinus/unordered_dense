#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

TEST_CASE_MAP("load_factor", int, int) {
    auto m = map_t();

    REQUIRE(static_cast<double>(m.load_factor()) == doctest::Approx(0.0));

    for (int i = 0; i < 10000; ++i) {
        m.emplace(i, i);
        REQUIRE(m.load_factor() > 0.0F);
        REQUIRE(m.load_factor() <= 0.8F);
    }
}

// Nothing checked that a max_load_factor the caller sets is still the one the table grows by: making
// allocate_buckets_from_shift() ignore max_load_factor() and hardcode the 0.8 default passed the whole suite. The
// setting is honoured, it just had no test, so it could have been lost in a refactoring without anything noticing.
TEST_CASE_MAP("max_load_factor_is_honoured_through_growth", int, int) {
    for (float requested : {0.25F, 0.5F, 0.9F}) {
        auto m = map_t();
        m.max_load_factor(requested);
        REQUIRE(m.max_load_factor() == requested);

        // Enough inserts to grow the table many times over, so this covers the sizing of every new bucket array and
        // not just the first one.
        for (int i = 0; i < 20000; ++i) {
            m.emplace(i, i);
            REQUIRE(m.load_factor() <= requested);
        }

        // The same thing said in terms of what was allocated: a table holding n elements at load factor f needs at
        // least n / f buckets.
        REQUIRE(static_cast<float>(m.bucket_count()) >= static_cast<float>(m.size()) / requested);
        REQUIRE(m.max_load_factor() == requested);
    }
}

// A load factor above 1 used to be accepted and then hang: the table filled completely and the next insert probed
// forever for an empty bucket. It is clamped now, so this test finishes rather than spinning.
TEST_CASE_MAP("max_load_factor_above_one_is_clamped", int, int) {
    auto m = map_t();
    m.max_load_factor(2.0F);
    REQUIRE(m.max_load_factor() <= 1.0F);

    // 1.0 means every bucket is used before the table grows, so this walks the completely full table at every size
    for (int i = 0; i < 10000; ++i) {
        m.emplace(i, i);
        REQUIRE(m.load_factor() <= 1.0F);
    }
    REQUIRE(m.size() == 10000U);
    for (int i = 0; i < 10000; ++i) {
        REQUIRE(m.find(i) != m.end());
    }

    // and the same table has to survive erasing, which shifts buckets down
    for (int i = 0; i < 10000; i += 2) {
        REQUIRE(m.erase(i) == 1U);
    }
    REQUIRE(m.size() == 5000U);
    for (int i = 1; i < 10000; i += 2) {
        REQUIRE(m.find(i) != m.end());
    }
}

// max_load_factor() does not only record the number, it also recomputes the size at which the table
// considers itself full -- otherwise the new setting would not take effect until the next time the
// bucket array was built. The tests above all set it on a table with no buckets yet, where that
// recomputation has nothing to do, so deleting it changed nothing.
//
// Setting it on a table that is already populated is the case that tells: demanding a load factor
// the table is already over has to make the very next insert grow it.
TEST_CASE_MAP("lowering_max_load_factor_applies_to_an_existing_bucket_array", int, int) {
    auto map = map_t();
    map.reserve(100);
    for (int i = 0; i < 50; ++i) {
        map.try_emplace(i, i);
    }
    auto const before = map.bucket_count();
    REQUIRE(map.load_factor() < 0.8F); // comfortably inside the default, so nothing is due to grow

    map.max_load_factor(0.1F);
    REQUIRE(map.max_load_factor() == 0.1F);

    // One insert buys one doubling, not however many it would take to get under 0.1 -- growth is a
    // single step per insert that finds the table full. So what is asserted is that the array grew
    // at all, which it would not have done if the new setting had gone unnoticed.
    map.try_emplace(1000, 1000);
    REQUIRE(map.bucket_count() > before);

    // and the table still holds everything it did
    REQUIRE(map.size() == 51);
    for (int i = 0; i < 50; ++i) {
        REQUIRE(map.find(i) != map.end());
    }
}
