#if 0
#include <boost/unordered/unordered_flat_map.hpp>

#include <doctest.h>

TEST_CASE("advance") {
    auto map = boost::unordered_flat_map<int, int>();
    map[1] = 2;
    map[2] = 3;

    std::advance(map.begin(), 10);
}
#endif