#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace {

// A hasher that is fine to copy but whose move operations can throw. Everything else about the map stays the default,
// so any potentially-throwing operation the map reports has to come from here.
struct throwing_hash {
    using is_avalanching = void;

    throwing_hash() = default;
    throwing_hash(throwing_hash const&) = default;
    throwing_hash(throwing_hash&& /*other*/) noexcept(false) {} // NOLINT(performance-noexcept-move-constructor)
    auto operator=(throwing_hash const&) -> throwing_hash& = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    auto operator=(throwing_hash&& /*other*/) noexcept(false) -> throwing_hash& {
        return *this;
    }
    ~throwing_hash() = default;

    auto operator()(int x) const -> std::uint64_t {
        return static_cast<std::uint64_t>(x);
    }
};

using plain_map = ankerl::unordered_dense::map<int, int>;
using throwing_map = ankerl::unordered_dense::map<int, int, throwing_hash>;

// The everyday map keeps the strong guarantees callers rely on: std::vector<map> has to move its elements on growth
// rather than copy them, and that only happens when the move assignment is noexcept.
static_assert(std::is_nothrow_move_assignable_v<plain_map>);
static_assert(noexcept(std::declval<plain_map&>().swap(std::declval<plain_map&>())));

// ...and a map whose hasher can throw has to say so. These were both noexcept(true) regardless, because the condition
// sat inside another noexcept(), which only ever asks whether evaluating a bool can throw.
static_assert(!std::is_nothrow_move_assignable_v<throwing_hash>);
static_assert(!std::is_nothrow_move_assignable_v<throwing_map>);
static_assert(!std::is_nothrow_swappable_v<throwing_hash>);
static_assert(!noexcept(std::declval<throwing_map&>().swap(std::declval<throwing_map&>())));

} // namespace

// The static_asserts above are the test; this only makes the translation unit do something at runtime, and checks that
// a map with such a hasher is otherwise perfectly usable.
TEST_CASE("noexcept_contract") {
    auto a = throwing_map();
    for (int i = 0; i < 100; ++i) {
        a[i] = i;
    }

    auto b = throwing_map();
    a.swap(b);
    REQUIRE(a.empty());
    REQUIRE(b.size() == 100U);

    auto c = std::move(b);
    REQUIRE(c.size() == 100U);
    REQUIRE(c.find(42) != c.end());
}
