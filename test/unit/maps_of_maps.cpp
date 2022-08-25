#include <ankerl/unordered_dense.h>

#include <app/checksum.h>
#include <app/counter.h>
#include <third-party/nanobench.h>

#include <doctest.h>

#include <cstddef> // for size_t
#include <cstdint> // for uint64_t
#include <utility> // for move

TEST_CASE("mapmap") {
    auto counts = counter();
    INFO(counts);

    using map_t = ankerl::unordered_dense::map<counter::obj, ankerl::unordered_dense::map<counter::obj, counter::obj>>;

    auto rng = ankerl::nanobench::Rng();
    for (size_t trial = 0; trial < 4; ++trial) {
        {
            counts("start");
            auto maps = map_t();
            for (size_t i = 0; i < 100; ++i) {
                auto a = rng.bounded(20);
                auto b = rng.bounded(20);
                auto x = static_cast<size_t>(rng());
                // std::cout << i << ": map[" << a << "][" << b << "] = " << x << std::endl;
                maps[counter::obj(a, counts)][counter::obj(b, counts)] = counter::obj(x, counts);
            }
            counts("filled");

            map_t mapsCopied;
            mapsCopied = maps;
            REQUIRE(checksum::mapmap(mapsCopied) == checksum::mapmap(maps));
            REQUIRE(mapsCopied == maps);
            counts("copied");

            map_t mapsMoved;
            mapsMoved = std::move(mapsCopied);
            counts("moved");

            // move
            REQUIRE(checksum::mapmap(mapsMoved) == checksum::mapmap(maps));
            REQUIRE(mapsCopied.size() == 0); // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
            mapsCopied = std::move(mapsMoved);
            counts("moved back");

            // move back
            REQUIRE(checksum::mapmap(mapsCopied) == checksum::mapmap(maps));
            REQUIRE(mapsMoved.size() == 0); // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
            counts("done");
        }
        counts("all destructed");
        REQUIRE(counts.dtor() ==
                counts.ctor() + counts.static_default_ctor + counts.copy_ctor() + counts.default_ctor() + counts.move_ctor());
    }
}
