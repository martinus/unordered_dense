#include <ankerl/unordered_dense.h>

#include <app/Counter.h>
#include <doctest.h>

#include <cstddef>       // for size_t
#include <cstdint>       // for uint64_t
#include <unordered_set> // for unordered_set
#include <utility>       // for pair
#include <vector>        // for vector

TEST_CASE("iterators_erase") {
    Counter counts;
    INFO(counts);
    {
        counts("begin");
        auto map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>();
        for (size_t i = 0; i < 100; ++i) {
            map[Counter::Obj(i * 101, counts)] = Counter::Obj(i * 101, counts);
        }

        auto it = map.find(Counter::Obj(size_t{20} * 101, counts));
        REQUIRE(map.size() == 100);
        REQUIRE(map.end() != map.find(Counter::Obj(size_t{20} * 101, counts)));
        it = map.erase(it);
        REQUIRE(map.size() == 99);
        REQUIRE(map.end() == map.find(Counter::Obj(size_t{20} * 101, counts)));

        it = map.begin();
        size_t currentSize = map.size();
        std::unordered_set<uint64_t> keys;
        while (it != map.end()) {
            REQUIRE(keys.emplace(it->first.get()).second);
            it = map.erase(it);
            currentSize--;
            REQUIRE(map.size() == currentSize);
        }
        REQUIRE(map.size() == static_cast<size_t>(0));
        counts("done");
    }
    counts("destructed");
    REQUIRE(counts.dtor == counts.ctor + counts.staticDefaultCtor + counts.copyCtor + counts.defaultCtor + counts.moveCtor);
}
