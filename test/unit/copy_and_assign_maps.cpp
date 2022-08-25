#include <ankerl/unordered_dense.h>

#define ENABLE_LOG_LINE
#include <app/print.h>

#include <doctest.h>

#include <utility> // for pair
#include <vector>  // for vector

using map_t = ankerl::unordered_dense::map<int, int>;

// creates a map with some data in it
template <class M>
[[nodiscard]] auto create_map(int num_elements) -> M {
    M m;
    for (int i = 0; i < num_elements; ++i) {
        m[static_cast<typename M::key_type>((i + 123) * 7)] = static_cast<typename M::mapped_type>(i);
    }
    return m;
}

TEST_CASE("copy_and_assign_maps") {
    { auto a = create_map<map_t>(15); }

    { auto a = create_map<map_t>(100); }

    {
        auto a = create_map<map_t>(1);
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
        auto b = a;
        REQUIRE(a == b);
    }

    {
        map_t a;
        REQUIRE(a.empty());
        a.clear();
        REQUIRE(a.empty());
    }

    {
        auto a = create_map<map_t>(100);
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
        auto b = a;
        REQUIRE(b == a);
    }
    {
        map_t a;
        a[123] = 321;
        a.clear();
        std::vector<map_t> maps(10, a);

        for (auto const& map : maps) {
            REQUIRE(map.empty());
        }
    }

    {
        std::vector<map_t> maps(10);
        REQUIRE(maps.size() == 10U);
    }

    {
        map_t a;
        std::vector<map_t> maps(12, a);
        REQUIRE(maps.size() == 12U);
    }

    {
        map_t a;
        a[123] = 321;
        std::vector<map_t> maps(10, a);
        a[123] = 1;

        for (auto const& map : maps) {
            REQUIRE(map.size() == 1);
            REQUIRE(map.find(123)->second == 321);
        }
    }
}
