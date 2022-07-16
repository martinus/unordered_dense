#include <ankerl/unordered_dense.h>

#define ENABLE_LOG_LINE
#include <app/print.h>

#include <doctest.h>

#include <utility> // for pair
#include <vector>  // for vector

using Map = ankerl::unordered_dense::map<int, int>;

// creates a map with some data in it
template <class M>
[[nodiscard]] auto createMap(int numElements) -> M {
    M m;
    for (int i = 0; i < numElements; ++i) {
        m[static_cast<typename M::key_type>((i + 123) * 7)] = static_cast<typename M::mapped_type>(i);
    }
    return m;
}

TEST_CASE("copy_and_assign_maps") {
    { auto a = createMap<Map>(15); }

    { auto a = createMap<Map>(100); }

    {
        auto a = createMap<Map>(1);
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
        auto b = a;
        REQUIRE(a == b);
    }

    {
        Map a;
        REQUIRE(a.empty());
        a.clear();
        REQUIRE(a.empty());
    }

    {
        auto a = createMap<Map>(100);
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
        auto b = a;
        REQUIRE(b == a);
    }
    {
        Map a;
        a[123] = 321;
        a.clear();
        std::vector<Map> maps(10, a);

        for (auto const& map : maps) {
            REQUIRE(map.empty());
        }
    }

    {
        std::vector<Map> maps(10);
        REQUIRE(maps.size() == 10U);
    }

    {
        Map a;
        std::vector<Map> maps(12, a);
        REQUIRE(maps.size() == 12U);
    }

    {
        Map a;
        a[123] = 321;
        std::vector<Map> maps(10, a);
        a[123] = 1;

        for (auto const& map : maps) {
            REQUIRE(map.size() == 1);
            REQUIRE(map.find(123)->second == 321);
        }
    }
}
