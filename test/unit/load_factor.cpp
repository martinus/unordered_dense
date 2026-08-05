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
