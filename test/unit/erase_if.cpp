#include <ankerl/unordered_dense.h>

#include <app/Counter.h>

#include <doctest.h>

using Set = ankerl::unordered_dense::set<Counter::Obj>;
using Map = ankerl::unordered_dense::set<Counter::Obj, Counter::Obj>;

TEST_CASE("erase_if_set") {
    Counter counts;
    INFO(counts);
    auto set = Set();

    for (size_t i = 0; i < 1000; ++i) {
        set.emplace(i, counts);
    }
    REQUIRE(set.size() == 1000);
    std::erase_if(set, [](Counter::Obj const& obj) {
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
