#include <ankerl/unordered_dense.h>

#include <app/Counter.h>
#include <app/nanobench.h>

#include <doctest.h>

#include <unordered_map>
#include <utility>

TEST_CASE("multiple_different_APIs" * doctest::test_suite("stochastic")) {
    using Map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>;
    Counter counts;
    INFO(counts);

    Map map;
    REQUIRE(map.size() == static_cast<size_t>(0));
    std::pair<typename Map::iterator, bool> it_outer = map.insert(typename Map::value_type{{32145, counts}, {123, counts}});
    REQUIRE(it_outer.second);
    REQUIRE(it_outer.first->first.get() == 32145);
    REQUIRE(it_outer.first->second.get() == 123);
    REQUIRE(map.size() == 1);

    const size_t times = 10000;
    for (size_t i = 0; i < times; ++i) {
        INFO(i);
        std::pair<typename Map::iterator, bool> it_inner = map.insert(typename Map::value_type({i * 4U, counts}, {i, counts}));

        REQUIRE(it_inner.second);
        REQUIRE(it_inner.first->first.get() == i * 4);
        REQUIRE(it_inner.first->second.get() == i);

        auto found = map.find(Counter::Obj{i * 4, counts});
        REQUIRE(map.end() != found);
        REQUIRE(found->second.get() == i);
        REQUIRE(map.size() == 2 + i);
    }

    // check if everything can be found
    for (size_t i = 0; i < times; ++i) {
        auto found = map.find(Counter::Obj{i * 4, counts});
        REQUIRE(map.end() != found);
        REQUIRE(found->second.get() == i);
        REQUIRE(found->first.get() == i * 4);
    }

    // check non-elements
    for (size_t i = 0; i < times; ++i) {
        auto found = map.find(Counter::Obj{(i + times) * 4U, counts});
        REQUIRE(map.end() == found);
    }

    // random test against std::unordered_map
    map.clear();
    std::unordered_map<uint64_t, uint64_t> uo;

    auto rng = ankerl::nanobench::Rng();
    INFO("seed=" << rng.state());

    for (uint64_t i = 0; i < times; ++i) {
        auto r = static_cast<size_t>(rng.bounded(times / 4));
        auto rhh_it = map.insert(typename Map::value_type({r, counts}, {r * 2, counts}));
        auto uo_it = uo.insert(std::make_pair(r, r * 2));
        REQUIRE(rhh_it.second == uo_it.second);
        REQUIRE(rhh_it.first->first.get() == uo_it.first->first);
        REQUIRE(rhh_it.first->second.get() == uo_it.first->second);
        REQUIRE(map.size() == uo.size());

        r = rng.bounded(times / 4);
        auto mapIt = map.find(Counter::Obj{r, counts});
        auto uoIt = uo.find(r);
        REQUIRE((map.end() == mapIt) == (uo.end() == uoIt));
        if (map.end() != mapIt) {
            REQUIRE(mapIt->first.get() == uoIt->first);
            REQUIRE(mapIt->second.get() == uoIt->second);
        }
    }

    uo.clear();
    map.clear();
    for (size_t i = 0; i < times; ++i) {
        const auto r = static_cast<size_t>(rng.bounded(times / 4));
        map[{r, counts}] = {r * 2, counts};
        uo[r] = r * 2;
        REQUIRE(map.find(Counter::Obj{r, counts})->second.get() == uo.find(r)->second);
        REQUIRE(map.size() == uo.size());
    }

    std::size_t numChecks = 0;
    for (auto it = map.begin(); it != map.end(); ++it) {
        REQUIRE(uo.end() != uo.find(it->first.get()));
        ++numChecks;
    }
    REQUIRE(map.size() == numChecks);

    numChecks = 0;
    const Map& constRhhs = map;
    for (const typename Map::value_type& vt : constRhhs) {
        REQUIRE(uo.end() != uo.find(vt.first.get()));
        ++numChecks;
    }
    REQUIRE(map.size() == numChecks);
}
