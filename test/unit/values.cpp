#include <ankerl/unordered_dense.h>

#include <app/counter.h>
#include <app/doctest.h>

#include <fmt/format.h>

TEST_CASE_MAP("values", counter::obj, counter::obj) {
    counter counts;
    INFO(counts);

    counts("start");
    size_t const n = 100;
    auto map = map_t();
    for (size_t i = 0; i < n; ++i) {
        map.emplace(counter::obj(i, counts), counter::obj(i, counts));
    }
    counts("filled");
    const auto cmap = map;
    counts("copyed");

    auto d1 = counts.data();
    auto values = std::move(map).values();
    counts("values() &&");
    auto cvalues = std::move(cmap).values();
    counts("values() const&&");
    auto d2 = counts.data();
    REQUIRE(d1 == d2);
    REQUIRE(values.size() == n);
    REQUIRE(cvalues.size() == n);

    map.clear();
    for (size_t i = 0; i < n; ++i) {
        map.emplace(counter::obj(i, counts), counter::obj(i, counts));
    }
    for (size_t i = 0; i < n; ++i) {
        REQUIRE(map.contains(counter::obj(i, counts)));
    }
}
