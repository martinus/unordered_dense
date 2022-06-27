#include <ankerl/unordered_dense_map.h>

#define ENABLE_LOG_LINE
#include <app/print.h>

#include <doctest.h>

TEST_CASE("move_to_moved") {
    auto a = ankerl::unordered_dense_map<int, int>();
    a[1] = 2;
    auto moved = std::move(a);

    auto c = ankerl::unordered_dense_map<int, int>();
    c[3] = 4;

    // assign to a moved map
    a = std::move(c);

    a[5] = 6;
    moved[6] = 7;
    REQUIRE(moved[6] == 7);
}
