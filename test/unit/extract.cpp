#include <ankerl/unordered_dense.h>
#include <app/Counter.h>

#include <doctest.h>

#if 0
TEST_CASE("extract") {
    Counter counts;
    INFO(counts);

    auto container = std::vector<std::pair<Counter::Obj, Counter::Obj>>();
    {
        auto map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>();

        for (size_t i = 0; i < 100; ++i) {
            map.try_emplace(Counter::Obj{i, counts}, i, counts);
        }

        container = std::move(map).extract();
    }

    REQUIRE(container.size() == 100U);
    for (size_t i = 0; i < container.size(); ++i) {
        REQUIRE(container[i].first.get() == i);
        REQUIRE(container[i].second.get() == i);
    }
}

#endif