#include <ankerl/unordered_dense.h>

#include <app/counter.h>

#include <doctest.h>

#include <cstddef> // for size_t
#include <cstdint> // for uint64_t
#include <utility> // for pair

using set_t = ankerl::unordered_dense::set<counter::obj>;

TEST_CASE("erase_if_set") {
    auto counts = counter();
    INFO(counts);
    auto set = set_t();

    for (size_t i = 0; i < 1000; ++i) {
        set.emplace(i, counts);
    }
    REQUIRE(set.size() == 1000);
    std::erase_if(set, [](counter::obj const& obj) {
        return 0 == obj.get() % 3;
    });

    REQUIRE(set.size() == 666);
    for (size_t i = 0; i < 1000; ++i) {
        if (0 == i % 3) {
            REQUIRE(!set.contains({i, counts}));
        } else {
            REQUIRE(set.contains({i, counts}));
        }
    }
}

TEST_CASE("erase_if_map") {
    auto map = ankerl::unordered_dense::map<uint64_t, uint64_t>();
    for (size_t i = 0; i < 1000; ++i) {
        map.try_emplace(i, i);
    }

    REQUIRE(map.size() == 1000);
    std::erase_if(map, [](std::pair<uint64_t, uint64_t> const& x) {
        return 0 == x.second % 2;
    });

    REQUIRE(map.size() == 500);
    for (size_t i = 0; i < 1000; ++i) {
        if (0 == i % 2) {
            REQUIRE(!map.contains(i));
        } else {
            REQUIRE(map.contains(i));
        }
    }
}
