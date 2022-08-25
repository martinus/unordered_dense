#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <cstddef> // for size_t
#include <vector>  // for vector

// struct that provides both hash and equals operator
struct hash_with_equal {
    auto operator()(int x) const -> size_t {
        return static_cast<size_t>(x);
    }

    auto operator()(int a, int b) const -> bool {
        return a == b;
    }
};

// make sure the map works with the same type (check that it handles the diamond problem)
TEST_CASE("diamond_problem") {
    auto map = ankerl::unordered_dense::map<int, int, hash_with_equal, hash_with_equal>();
    map[1] = 2;
    REQUIRE(map.size() == 1);
    REQUIRE(map.find(1) != map.end());
}
