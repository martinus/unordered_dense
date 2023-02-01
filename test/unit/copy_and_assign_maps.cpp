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

TEST_CASE("copy_and_assign_maps_1") {
    auto a = create_map<map_t>(15);
}

TEST_CASE("copy_and_assign_maps_2") {
    auto a = create_map<map_t>(100);
}

TEST_CASE("copy_and_assign_maps_3") {
    auto a = create_map<map_t>(1);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    auto b = a;
    REQUIRE(a == b);
}

TEST_CASE("copy_and_assign_maps_4") {
    map_t a;
    REQUIRE(a.empty());
    a.clear();
    REQUIRE(a.empty());
}

TEST_CASE("copy_and_assign_maps_5") {
    auto a = create_map<map_t>(100);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    auto b = a;
    REQUIRE(b == a);
}

TEST_CASE("copy_and_assign_maps_6") {
    map_t a;
    a[123] = 321;
    a.clear();
    std::vector<map_t> maps(10, a);

    for (auto const& map : maps) {
        REQUIRE(map.empty());
    }
}

TEST_CASE("copy_and_assign_maps_7") {
    std::vector<map_t> maps(10);
    REQUIRE(maps.size() == 10U);
}

TEST_CASE("copy_and_assign_maps_8") {
    map_t a;
    std::vector<map_t> maps(12, a);
    REQUIRE(maps.size() == 12U);
}

TEST_CASE("copy_and_assign_maps_9") {
    map_t a;
    a[123] = 321;
    std::vector<map_t> maps(10, a);
    a[123] = 1;

    for (auto const& map : maps) {
        REQUIRE(map.size() == 1);
        REQUIRE(map.find(123)->second == 321);
    }
}
