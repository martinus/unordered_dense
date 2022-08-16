#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <deque>

#if 0
static_assert(
    ankerl::unordered_dense::detail::is_detected_v<ankerl::unordered_dense::detail::detect_iterator, std::deque<int>>);

static_assert(
    !ankerl::unordered_dense::detail::is_detected_v<ankerl::unordered_dense::detail::detect_iterator, std::allocator<int>>);

TEST_CASE("custom_container") {
    using Map = ankerl::unordered_dense::
        map<int, std::string, ankerl::unordered_dense::hash<int>, std::equal_to<int>, std::deque<std::pair<int, std::string>>>;

    auto map = Map();

    for (int i = 0; i < 10; ++i) {
        map[i] = std::to_string(i);
    }

    REQUIRE(std::is_same_v<std::deque<std::pair<int, std::string>>, typename Map::value_container_type>);
    std::deque<std::pair<int, std::string>> container = std::move(map).extract();
}

#endif