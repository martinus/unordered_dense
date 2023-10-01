#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

namespace {

struct my_foo {
    int m_i;
};

struct my_foo_avalanching {
    int m_i;
};

} // namespace

namespace std {

template <>
struct hash<my_foo> {
    auto operator()(my_foo const& mf) const -> size_t {
        return std::hash<int>{}(mf.m_i);
    }
};

template <>
struct hash<my_foo_avalanching> {
    using is_avalanching = void;

    auto operator()(my_foo const& mf) const -> size_t {
        return std::hash<int>{}(mf.m_i);
    }
};

} // namespace std

static_assert(
    !ankerl::unordered_dense::detail::is_detected_v<ankerl::unordered_dense::detail::detect_avalanching, std::hash<my_foo>>);
static_assert(ankerl::unordered_dense::detail::is_detected_v<ankerl::unordered_dense::detail::detect_avalanching,
                                                             std::hash<my_foo_avalanching>>);

static_assert(!ankerl::unordered_dense::detail::is_detected_v<ankerl::unordered_dense::detail::detect_avalanching,
                                                              ankerl::unordered_dense::hash<my_foo>>);
static_assert(ankerl::unordered_dense::detail::is_detected_v<ankerl::unordered_dense::detail::detect_avalanching,
                                                             ankerl::unordered_dense::hash<my_foo_avalanching>>);
