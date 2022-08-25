#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <stdexcept> // for out_of_range

using map_t = ankerl::unordered_dense::map<int, int>;

TEST_CASE("at") {
    map_t map;
    map_t const& cmap = map;

    // NOLINTNEXTLINE(llvm-else-after-return,readability-else-after-return)
    REQUIRE_THROWS_AS(map.at(123), std::out_of_range);

    // NOLINTNEXTLINE(llvm-else-after-return,readability-else-after-return)
    REQUIRE_THROWS_AS(static_cast<void>(map.at(0)), std::out_of_range);

    // NOLINTNEXTLINE(llvm-else-after-return,readability-else-after-return)
    REQUIRE_THROWS_AS(static_cast<void>(cmap.at(123)), std::out_of_range);

    // NOLINTNEXTLINE(llvm-else-after-return,readability-else-after-return)
    REQUIRE_THROWS_AS(static_cast<void>(cmap.at(0)), std::out_of_range);

    map[123] = 333;
    REQUIRE(map.at(123) == 333);
    REQUIRE(cmap.at(123) == 333);

    // NOLINTNEXTLINE(llvm-else-after-return,readability-else-after-return)
    REQUIRE_THROWS_AS(static_cast<void>(map.at(0)), std::out_of_range);

    // NOLINTNEXTLINE(llvm-else-after-return,readability-else-after-return)
    REQUIRE_THROWS_AS(static_cast<void>(cmap.at(0)), std::out_of_range);
}
