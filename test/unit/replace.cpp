#include <ankerl/unordered_dense.h>

#include <app/counter.h>

#include <doctest.h>

TEST_CASE("replace") {
    auto counts = counter{};
    INFO(counts);

    using map_t = ankerl::unordered_dense::map<counter::obj, counter::obj>;
    auto container = map_t::value_container_type{};

    for (size_t i = 0; i < 100; ++i) {
        container.emplace_back(counter::obj{i, counts}, counter::obj{i, counts});
        container.emplace_back(counter::obj{i, counts}, counter::obj{i, counts});
    }

    for (size_t i = 0; i < 10; ++i) {
        container.emplace_back(counter::obj{i, counts}, counter::obj{i, counts});
    }

    // add some elements
    auto map = map_t();
    for (size_t i = 0; i < 10; ++i) {
        map.try_emplace(counter::obj{i, counts}, counter::obj{i, counts});
    }

    map.replace(std::move(container));

    REQUIRE(map.size() == 100U);
    for (size_t i = 0; i < 100; ++i) {
        REQUIRE(map.contains(counter::obj{i, counts}));
    }
}
