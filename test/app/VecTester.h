#pragma once

#include <ankerl/svector.h>

#include <doctest.h>
#include <fmt/format.h>

#include <cstddef>
#include <stdexcept>
#include <vector>

template <typename VecA, typename VecB>
void assert_eq(VecA const& a, VecB const& b) {
    if (a.size() != b.size()) {
        throw std::runtime_error(fmt::format("vec size != svec size: {} != {}", a.size(), b.size()));
    }
    if (!std::equal(a.begin(), a.end(), b.begin(), b.end())) {
        throw std::runtime_error(
            fmt::format("vec content != svec content:\n[{}]\n[{}]", fmt::join(a, ","), fmt::join(b, ",")));
    }
}

template <class T, size_t N>
class VecTester {
    std::vector<T> m_v{};
    ankerl::svector<T, N> m_s{};

public:
    template <class... Args>
    void emplace_back(Args&&... args) {
        m_v.emplace_back(std::forward<Args>(args)...);
        m_s.emplace_back(std::forward<Args>(args)...);
        assert_eq(m_v, m_s);
    }

    template <class... Args>
    void emplace_at(size_t idx, Args&&... args) {
        auto it_v = m_v.emplace(m_v.begin() + idx, std::forward<Args>(args)...);
        auto it_s = m_s.emplace(m_s.cbegin() + idx, std::forward<Args>(args)...);
        REQUIRE(*it_v == *it_s);
        assert_eq(m_v, m_s);
    }

    [[nodiscard]] auto size() const -> size_t {
        return m_v.size();
    }
};
