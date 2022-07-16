#include <ankerl/unordered_dense.h>

#define ENABLE_LOG_LINE
#include <app/print.h>

#include <doctest.h>

#include <type_traits> // for remove_reference, remove_referen...
#include <utility>     // for move

TEST_CASE("move_to_moved") {
    auto a = ankerl::unordered_dense::map<int, int>();
    a[1] = 2;
    auto moved = std::move(a);

    auto c = ankerl::unordered_dense::map<int, int>();
    c[3] = 4;

    // assign to a moved map
    a = std::move(c);

    a[5] = 6;
    moved[6] = 7;
    REQUIRE(moved[6] == 7);
}
