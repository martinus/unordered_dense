#include <ankerl/unordered_dense_map.h>

#include <app/Counter.h>
#include <app/checksum.h>
#include <app/nanobench.h>

#include <doctest.h>

TEST_CASE("mapmap") {
    Counter counts;
    INFO(counts);

    using M = ankerl::unordered_dense_map<Counter::Obj, ankerl::unordered_dense_map<Counter::Obj, Counter::Obj>>;

    auto rng = ankerl::nanobench::Rng();
    for (size_t trial = 0; trial < 4; ++trial) {
        {
            counts("start");
            auto maps = M();
            for (size_t i = 0; i < 100; ++i) {
                auto a = rng.bounded(20);
                auto b = rng.bounded(20);
                auto x = rng();
                // std::cout << i << ": map[" << a << "][" << b << "] = " << x << std::endl;
                maps[Counter::Obj(a, counts)][Counter::Obj(b, counts)] = Counter::Obj(x, counts);
            }
            counts("filled");

            M mapsCopied;
            mapsCopied = maps;
            REQUIRE(checksum::mapmap(mapsCopied) == checksum::mapmap(maps));
            REQUIRE(mapsCopied == maps);
            counts("copied");

            M mapsMoved;
            mapsMoved = std::move(mapsCopied);
            counts("moved");

            // move
            REQUIRE(checksum::mapmap(mapsMoved) == checksum::mapmap(maps));
            REQUIRE(mapsCopied.size() == 0); // NOLINT(bugprone-use-after-move)
            mapsCopied = std::move(mapsMoved);
            counts("moved back");

            // move back
            REQUIRE(checksum::mapmap(mapsCopied) == checksum::mapmap(maps));
            REQUIRE(mapsMoved.size() == 0); // NOLINT(bugprone-use-after-move)
            counts("done");
        }
        counts("all destructed");
        REQUIRE(counts.dtor ==
                counts.ctor + counts.copyCtor + counts.staticDefaultCtor + counts.defaultCtor + counts.moveCtor);
    }
}
