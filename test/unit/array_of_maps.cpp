#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <array> // for array

TEST_CASE("array_of_maps") {

    // error? This should work.
    ankerl::unordered_dense::segmented_map<int, int> maps_ary2[2];

    // no errors
    ankerl::unordered_dense::segmented_map<int, int> maps_ary1[1];
    std::array<ankerl::unordered_dense::segmented_map<int, int>, 2> maps;
}