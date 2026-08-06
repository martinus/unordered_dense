#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/map_fixtures.h>

#include <cstddef>
#include <stdexcept>
#include <utility>

// Assignment gives the buckets back before it knows whether it can build new ones. A throw in
// between used to leave the table holding values with no bucket array: size() counted elements
// find() could not reach, and the next lookup indexed a container of length zero.
namespace {

// Throws on the nth copy, and only on copies -- the table has to be able to move it around while
// putting itself back together.
struct throws_on_copy {
    static inline int copies_until_throw = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    int m_value = 0;

    throws_on_copy() = default;

    explicit throws_on_copy(int value)
        : m_value(value) {}

    throws_on_copy(throws_on_copy const& other)
        : m_value(other.m_value) {
        if (0 < copies_until_throw && 0 == --copies_until_throw) {
            throw std::runtime_error("copy");
        }
    }

    throws_on_copy(throws_on_copy&&) noexcept = default;
    auto operator=(throws_on_copy&&) noexcept -> throws_on_copy& = default;
    ~throws_on_copy() = default;

    auto operator=(throws_on_copy const& other) -> throws_on_copy& {
        if (this != &other) {
            auto tmp = other; // may throw, which is the point
            m_value = tmp.m_value;
        }
        return *this;
    }
};

using map_t = ankerl::unordered_dense::map<int, throws_on_copy>;
using segmented_map_t = ankerl::unordered_dense::segmented_map<int, throws_on_copy>;

struct countdown_guard {
    explicit countdown_guard(int n) {
        throws_on_copy::copies_until_throw = n;
    }
    countdown_guard(countdown_guard const&) = delete;
    countdown_guard(countdown_guard&&) = delete;
    auto operator=(countdown_guard const&) -> countdown_guard& = delete;
    auto operator=(countdown_guard&&) -> countdown_guard& = delete;
    ~countdown_guard() {
        throws_on_copy::copies_until_throw = 0;
    }
};

template <typename Map>
void check_throwing_copy_assignment(int source_size, int target_size, int copies_until_throw) {
    auto source = test::filled<Map>(source_size);
    auto target = test::filled<Map>(target_size);

    {
        auto const guard = countdown_guard(copies_until_throw);
        REQUIRE_THROWS_AS(target = source, std::runtime_error);
    }

    test::require_empty_table_answers(target);
    // The source is untouched -- it was only ever read from.
    REQUIRE(source.size() == static_cast<std::size_t>(source_size));
}

} // namespace

TEST_CASE("copy_assignment_that_throws_leaves_a_usable_table") {
    SUBCASE("map") {
        check_throwing_copy_assignment<map_t>(200, 50, 100);
    }

    SUBCASE("segmented map") {
        check_throwing_copy_assignment<segmented_map_t>(200, 50, 100);
    }

    // Throwing on the very first copy, before the values container has committed to anything.
    SUBCASE("on the first copy") {
        check_throwing_copy_assignment<map_t>(200, 50, 1);
    }

    // Assigning onto a table that has no buckets to give back in the first place.
    SUBCASE("onto an empty table") {
        check_throwing_copy_assignment<map_t>(200, 0, 100);
    }
}
