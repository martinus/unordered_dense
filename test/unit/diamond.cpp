#include <ankerl/unordered_dense.h>

#include <doctest.h>

// struct that provides both hash and equals operator
struct HashWithEqual {
    auto operator()(int x) const -> size_t {
        return static_cast<size_t>(x);
    }

    auto operator()(int a, int b) const -> bool {
        return a == b;
    }
};

// make sure the map works with the same type (check that it handles the diamond problem)
TEST_CASE("diamond_problem") {
    ankerl::unordered_dense::map<int, int, HashWithEqual, HashWithEqual> map;
    map[1] = 2;
    REQUIRE(map.size() == 1);
    REQUIRE(map.find(1) != map.end());
}
