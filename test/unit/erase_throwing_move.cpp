#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstdint>
#include <stdexcept>

namespace {

// A value whose move constructor throws when armed. extract() hands the erased value to a callback that moves it into
// the caller's storage, and that happens after the bucket has already been shifted down: if the move throws there and
// nothing puts the table back together, m_values keeps an element that no bucket points at, so size() counts an
// element that cannot be found.
struct bomb {
    int m_value = 0;
    static inline bool s_armed = false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    bomb() = default;

    explicit bomb(int value)
        : m_value(value) {}

    bomb(bomb const& other) = default;

    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    bomb(bomb&& other) noexcept(false)
        : m_value(other.m_value) {
        if (s_armed) {
            throw std::runtime_error("boom");
        }
    }

    auto operator=(bomb const& other) -> bomb& = default;

    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    auto operator=(bomb&& other) noexcept(false) -> bomb& {
        m_value = other.m_value;
        return *this;
    }

    ~bomb() = default;

    auto operator==(bomb const& other) const -> bool {
        return m_value == other.m_value;
    }
};

struct bomb_hash {
    using is_avalanching = void;
    auto operator()(bomb const& b) const noexcept -> std::uint64_t {
        return ankerl::unordered_dense::hash<int>{}(b.m_value);
    }
};

template <typename Map>
void check_consistent(Map const& map, size_t expected_size) {
    REQUIRE(map.size() == expected_size);

    auto iterated = size_t();
    for (auto const& entry : map) {
        (void)entry;
        ++iterated;
    }
    REQUIRE(iterated == expected_size);

    auto findable = size_t();
    for (int i = 0; i < 100; ++i) {
        if (map.find(bomb{i}) != map.end()) {
            ++findable;
        }
    }
    REQUIRE(findable == expected_size);
}

} // namespace

TEST_CASE("erase_survives_a_throwing_move") {
    using map_t = ankerl::unordered_dense::map<bomb, int, bomb_hash>;

    auto map = map_t();
    for (int i = 0; i < 50; ++i) {
        map.try_emplace(bomb{i}, i);
    }
    check_consistent(map, 50);

    // extract(key): the throw happens while moving the value into the returned optional
    bomb::s_armed = true;
    REQUIRE_THROWS_AS(map.extract(bomb{17}), std::runtime_error);
    bomb::s_armed = false;
    check_consistent(map, 49);
    REQUIRE(map.find(bomb{17}) == map.end());

    // extract(iterator), same thing through the other entry point
    auto it = map.find(bomb{33});
    REQUIRE(it != map.end());
    bomb::s_armed = true;
    REQUIRE_THROWS_AS(map.extract(it), std::runtime_error);
    bomb::s_armed = false;
    check_consistent(map, 48);
    REQUIRE(map.find(bomb{33}) == map.end());

    // erasing the last value, the case where nothing has to be moved into the hole
    bomb::s_armed = true;
    REQUIRE_THROWS_AS(map.extract(bomb{49}), std::runtime_error);
    bomb::s_armed = false;
    check_consistent(map, 47);

    // the table still works afterwards
    for (int i = 100; i < 150; ++i) {
        map.try_emplace(bomb{i}, i);
    }
    REQUIRE(map.size() == 97U);
    for (int i = 100; i < 150; ++i) {
        REQUIRE(map.find(bomb{i}) != map.end());
    }
    REQUIRE(map.erase(bomb{0}) == 1U);
    REQUIRE(map.size() == 96U);
}
