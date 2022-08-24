#include <ankerl/unordered_dense.h>

#include <app/Counter.h>

#include <doctest.h>

TEST_CASE("replace") {
    auto counts = Counter{};
    INFO(counts);

    using Map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>;
    auto container = Map::value_container_type{};

    for (size_t i = 0; i < 100; ++i) {
        container.emplace_back(Counter::Obj{i, counts}, Counter::Obj{i, counts});
        container.emplace_back(Counter::Obj{i, counts}, Counter::Obj{i, counts});
    }

    for (size_t i = 0; i < 10; ++i) {
        container.emplace_back(Counter::Obj{i, counts}, Counter::Obj{i, counts});
    }

    // add some elements
    auto map = Map();
    for (size_t i = 0; i < 10; ++i) {
        map.try_emplace(Counter::Obj{i, counts}, Counter::Obj{i, counts});
    }

    map.replace(std::move(container));

    REQUIRE(map.size() == 100U);
    for (size_t i = 0; i < 100; ++i) {
        REQUIRE(map.contains(Counter::Obj{i, counts}));
    }
}
