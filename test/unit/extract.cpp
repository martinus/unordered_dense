#include <ankerl/unordered_dense.h>

#include <app/counter.h>
#include <app/doctest.h>

TEST_CASE_MAP("extract", counter::obj, counter::obj) {
    auto counts = counter();
    INFO(counts);

    auto container = typename map_t::value_container_type();
    {
        auto map = map_t();

        for (size_t i = 0; i < 100; ++i) {
            map.try_emplace(counter::obj{i, counts}, i, counts);
        }

        container = std::move(map).extract();
    }

    REQUIRE(container.size() == 100U);
    for (size_t i = 0; i < container.size(); ++i) {
        REQUIRE(container[i].first.get() == i);
        REQUIRE(container[i].second.get() == i);
    }
}
