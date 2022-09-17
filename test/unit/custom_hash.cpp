#include <ankerl/unordered_dense.h>

#include <doctest.h>
#include <type_traits>

namespace {

struct id {
    uint64_t value{};

    auto operator==(id const& other) const -> bool {
        return value == other.value;
    }
};

struct custom_hash_simple {
    [[nodiscard]] auto operator()(id const& x) const noexcept -> uint64_t {
        return x.value;
    }
};

struct custom_hash_avalanching {
    using is_avalanching = void;

    auto operator()(id const& x) const noexcept -> uint64_t {
        return ankerl::unordered_dense::detail::wyhash::hash(x.value);
    }
};

struct point {
    int x{};
    int y{};

    auto operator==(point const& other) const -> bool {
        return x == other.x && y == other.y;
    }
};

struct custom_hash_unique_object_representation {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(point const& f) const noexcept -> uint64_t {
        static_assert(std::has_unique_object_representations_v<point>);
        return ankerl::unordered_dense::detail::wyhash::hash(&f, sizeof(f));
    }
};

} // namespace

template <>
struct ankerl::unordered_dense::hash<id> {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(id const& x) const noexcept -> uint64_t {
        return detail::wyhash::hash(x.value);
    }
};

TEST_CASE("custom_hash") {
    {
        auto set = ankerl::unordered_dense::set<id, custom_hash_simple>();
        set.insert(id{124});
    }
    {
        auto set = ankerl::unordered_dense::set<id, custom_hash_avalanching>();
        set.insert(id{124});
    }
    {
        auto set = ankerl::unordered_dense::set<point, custom_hash_unique_object_representation>();
        set.insert(point{123, 321});
    }
    {
        auto set = ankerl::unordered_dense::set<id>();
        set.insert(id{124});
    }
}

static_assert(
    !ankerl::unordered_dense::detail::is_detected_v<ankerl::unordered_dense::detail::detect_avalanching, custom_hash_simple>);

static_assert(ankerl::unordered_dense::detail::is_detected_v<ankerl::unordered_dense::detail::detect_avalanching,
                                                             custom_hash_avalanching>);
static_assert(ankerl::unordered_dense::detail::is_detected_v<ankerl::unordered_dense::detail::detect_avalanching,
                                                             custom_hash_unique_object_representation>);

static_assert(!ankerl::unordered_dense::detail::is_detected_v<ankerl::unordered_dense::detail::detect_avalanching,
                                                              ankerl::unordered_dense::hash<point>>);
