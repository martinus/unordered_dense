#include <ankerl/unordered_dense.h>

#include <app/Counter.h>
#include <app/nanobench.h>

#include <doctest.h>

#include <algorithm> // for max
#include <cstddef>   // for size_t
#include <utility>   // for move
#include <vector>    // for vector

template <typename Map>
void fill(Counter& counts, Map& map, ankerl::nanobench::Rng& rng) {
    auto n = rng.bounded(20);
    for (size_t i = 0; i < n; ++i) {
        auto a = rng.bounded(20);
        auto b = rng.bounded(20);
        map.try_emplace({a, counts}, b, counts);
    }
}

TEST_CASE("vectormap") {
    Counter counts;
    INFO(counts);

    using Map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>;
    auto rng = ankerl::nanobench::Rng(32154);
    {
        counts("begin");
        std::vector<Map> maps;

        // copies
        for (size_t i = 0; i < 10; ++i) {
            Map m;
            fill(counts, m, rng);
            maps.push_back(m);
        }
        counts("copies");

        // move
        for (size_t i = 0; i < 10; ++i) {
            Map m;
            fill(counts, m, rng);
            maps.push_back(std::move(m));
        }
        counts("move");

        // emplace
        for (size_t i = 0; i < 10; ++i) {
            maps.emplace_back();
            fill(counts, maps.back(), rng);
        }
        counts("emplace");
    }
    counts("dtor");
    REQUIRE(counts.dtor == counts.ctor + counts.staticDefaultCtor + counts.copyCtor + counts.defaultCtor + counts.moveCtor);
}
