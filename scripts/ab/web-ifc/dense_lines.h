#pragma once
// IfcLoader::_lines as a vector indexed by express ID (unordered_dense #320). A slot whose ifcType is 0 is empty:
// ParseLines only stores lines with a type.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

template <class Line>
class dense_lines {
    std::vector<Line> m_v;
    std::size_t m_size = 0;

public:
    class iterator {
        dense_lines const* m_d;
        std::uint32_t m_i;
        mutable std::pair<std::uint32_t, Line> m_kv;
        void skip() {
            while (m_i < m_d->m_v.size() && m_d->m_v[m_i].ifcType == 0)
                ++m_i;
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<std::uint32_t, Line>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
        iterator(dense_lines const* d, std::uint32_t i)
            : m_d(d)
            , m_i(i) {
            skip();
        }
        reference operator*() const {
            m_kv = {m_i, m_d->m_v[m_i]};
            return m_kv;
        }
        pointer operator->() const {
            return &**this;
        }
        iterator& operator++() {
            ++m_i;
            skip();
            return *this;
        }
        bool operator==(iterator const& o) const {
            return m_i == o.m_i;
        }
        bool operator!=(iterator const& o) const {
            return m_i != o.m_i;
        }
    };

    iterator begin() const {
        return {this, 0};
    }
    iterator end() const {
        return {this, static_cast<std::uint32_t>(m_v.size())};
    }
    bool contains(std::uint32_t id) const {
        return id < m_v.size() && m_v[id].ifcType != 0;
    }
    iterator find(std::uint32_t id) const {
        return contains(id) ? iterator(this, id) : end();
    }
    Line& operator[](std::uint32_t id) {
        if (id >= m_v.size())
            m_v.resize(std::max<std::size_t>(id + 1, m_v.size() * 2));
        if (m_v[id].ifcType == 0)
            ++m_size; // every caller assigns a typed line right away
        return m_v[id];
    }
    void erase(std::uint32_t id) {
        if (contains(id)) {
            m_v[id] = Line();
            --m_size;
        }
    }
    void reserve(std::size_t n) {
        m_v.reserve(n);
    }
    void clear() {
        m_v.clear();
        m_size = 0;
    }
    std::size_t size() const {
        return m_size;
    }
};
