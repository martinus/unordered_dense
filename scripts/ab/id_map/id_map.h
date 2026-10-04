// id_map prototype (#379): integer IDs to values, for callers who know their IDs are nearly dense.
//
// Two layouts, both a flat directory indexed by id >> block_bits and nothing deeper:
//
// paged_map<K, T, BlockBits>: a directory of page pointers; a page holds a presence bitmap and one
// value slot per ID. A lookup loads the page pointer, then the bit and the slot, both of which depend
// only on the page pointer. A gap costs a slot.
//
// packed_map<K, T>: a directory of 64 byte block headers, one per 256 IDs: four bitmap words, the
// number of entries before each word, and a pointer to the block's values packed in ID order. A
// lookup loads the header, then the value at the rank its bitmap gives. A gap costs a bit.
//
// Directory entries without a block point to one shared empty block, so a lookup has no null check.
// Both iterate in ID order. emplace() on a present ID does nothing (try_emplace). The directory costs
// 8 (paged) or 64 (packed, per 256 IDs) bytes for every block of the range up to the largest ID.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace idm {

namespace detail {

inline auto popcount(std::uint64_t x) -> unsigned {
    return static_cast<unsigned>(__builtin_popcountll(x));
}
inline auto ctz(std::uint64_t x) -> unsigned {
    return static_cast<unsigned>(__builtin_ctzll(x));
}

// What an iterator hands out: the ID and a reference to its value, as it->first and it->second.
template <class K, class T>
struct ref {
    K first;
    T& second;
};

// Iteration in ID order over any table that offers next_at_or_after(id, &id) and at(id).
template <class Table, class K, class T>
class iterator {
    Table* m_t;
    K m_id;
    bool m_end;
    alignas(ref<K, T>) mutable unsigned char m_ref[sizeof(ref<K, T>)];

public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ref<K, T>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    iterator(Table* t, K id, bool end)
        : m_t(t)
        , m_id(id)
        , m_end(end) {}
    auto operator*() const -> reference {
        return *::new (static_cast<void*>(m_ref)) value_type{m_id, m_t->at(m_id)};
    }
    auto operator->() const -> pointer {
        return &**this;
    }
    auto operator++() -> iterator& {
        m_end = m_id == std::numeric_limits<K>::max() || !m_t->next_at_or_after(static_cast<K>(m_id + 1), &m_id);
        return *this;
    }
    auto operator==(iterator const& o) const -> bool {
        return m_end == o.m_end && (m_end || m_id == o.m_id);
    }
    auto operator!=(iterator const& o) const -> bool {
        return !(*this == o);
    }
};

// Iteration in ID order handing out the stored element itself (a std::pair<K, T>&), for a table
// that offers next_at_or_after(id, &id) and find_elem(id).
template <class Table, class K, class V>
class pair_iterator {
    Table* m_t;
    K m_id;
    bool m_end;

public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::remove_const_t<V>;
    using difference_type = std::ptrdiff_t;
    using pointer = V*;
    using reference = V&;

    pair_iterator(Table* t, K id, bool end)
        : m_t(t)
        , m_id(id)
        , m_end(end) {}
    auto operator*() const -> reference {
        return *m_t->find_elem(m_id);
    }
    auto operator->() const -> pointer {
        return m_t->find_elem(m_id);
    }
    auto operator++() -> pair_iterator& {
        m_end = m_id == std::numeric_limits<K>::max() || !m_t->next_at_or_after(static_cast<K>(m_id + 1), &m_id);
        return *this;
    }
    auto operator==(pair_iterator const& o) const -> bool {
        return m_end == o.m_end && (m_end || m_id == o.m_id);
    }
    auto operator!=(pair_iterator const& o) const -> bool {
        return !(*this == o);
    }
};

} // namespace detail

template <class K, class T, unsigned BlockBits = 8>
class paged_map {
    static_assert(std::is_unsigned_v<K> || std::is_signed_v<K>, "K must be an integer");
    static constexpr std::size_t slots = std::size_t{1} << BlockBits;
    static constexpr std::size_t words = (slots + 63) / 64;
    static constexpr std::size_t mask = slots - 1;

    struct page {
        std::uint64_t bits[words];
        alignas(T) unsigned char storage[slots * sizeof(T)];
        auto slot(std::size_t i) -> T* {
            return std::launder(reinterpret_cast<T*>(storage) + i);
        }
        auto has(std::size_t i) const -> bool {
            return ((bits[i / 64] >> (i % 64)) & 1U) != 0;
        }
    };

    // read by every lookup into a block that has none; its bits stay zero, its slots are never touched
    static inline page s_empty{};

    std::vector<page*> m_dir;
    std::size_t m_size = 0;

    static auto idx(K id) -> std::size_t {
        return static_cast<std::size_t>(static_cast<std::make_unsigned_t<K>>(id));
    }

    auto page_for_insert(std::size_t p) -> page* {
        if (p >= m_dir.size()) {
            m_dir.resize(p + 1, &s_empty);
        }
        if (m_dir[p] == &s_empty) {
            auto* pg = static_cast<page*>(::operator new(sizeof(page)));
            std::memset(pg->bits, 0, sizeof(pg->bits));
            m_dir[p] = pg;
        }
        return m_dir[p];
    }

    void destroy_all() {
        for (auto* pg : m_dir) {
            if (pg == &s_empty) {
                continue;
            }
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (std::size_t w = 0; w < words; ++w) {
                    for (auto b = pg->bits[w]; b != 0; b &= b - 1) {
                        pg->slot(w * 64 + detail::ctz(b))->~T();
                    }
                }
            }
            ::operator delete(pg);
        }
        m_dir.clear();
        m_size = 0;
    }

public:
    using key_type = K;
    using mapped_type = T;
    using iterator = detail::iterator<paged_map, K, T>;
    using const_iterator = detail::iterator<paged_map const, K, T const>;

    paged_map() = default;
    explicit paged_map(std::size_t /*bucket_count*/) {}
    paged_map(paged_map const& o) {
        for (std::size_t p = 0; p < o.m_dir.size(); ++p) {
            auto* src = o.m_dir[p];
            for (std::size_t w = 0; w < words && src != &s_empty; ++w) {
                for (auto b = src->bits[w]; b != 0; b &= b - 1) {
                    auto i = w * 64 + detail::ctz(b);
                    emplace(static_cast<K>((p << BlockBits) | i), *src->slot(i));
                }
            }
        }
    }
    paged_map(paged_map&& o) noexcept
        : m_dir(std::move(o.m_dir))
        , m_size(std::exchange(o.m_size, 0)) {}
    auto operator=(paged_map o) -> paged_map& {
        std::swap(m_dir, o.m_dir);
        std::swap(m_size, o.m_size);
        return *this;
    }
    ~paged_map() {
        destroy_all();
    }

    // nullptr if absent: the hot path, one directory load, then the bit and the slot
    auto find_ptr(K id) const -> T* {
        auto const i = idx(id);
        auto const p = i >> BlockBits;
        if (p >= m_dir.size()) {
            return nullptr;
        }
        auto* pg = m_dir[p];
        auto const s = i & mask;
        return pg->has(s) ? pg->slot(s) : nullptr;
    }
    auto contains(K id) const -> bool {
        return find_ptr(id) != nullptr;
    }
    auto find(K id) -> iterator {
        return find_ptr(id) != nullptr ? iterator(this, id, false) : end();
    }
    auto find(K id) const -> const_iterator {
        return find_ptr(id) != nullptr ? const_iterator(this, id, false) : end();
    }

    template <class... Args>
    auto try_emplace(K id, Args&&... args) -> std::pair<iterator, bool> {
        auto const i = idx(id);
        auto* pg = page_for_insert(i >> BlockBits);
        auto const s = i & mask;
        if (pg->has(s)) {
            return {iterator(this, id, false), false};
        }
        ::new (static_cast<void*>(pg->slot(s))) T(std::forward<Args>(args)...);
        pg->bits[s / 64] |= std::uint64_t{1} << (s % 64);
        ++m_size;
        return {iterator(this, id, false), true};
    }
    template <class V>
    auto emplace(K id, V&& v) -> std::pair<iterator, bool> {
        return try_emplace(id, std::forward<V>(v));
    }
    auto operator[](K id) -> T& {
        try_emplace(id);
        return *find_ptr(id);
    }

    auto erase(K id) -> std::size_t {
        auto* v = find_ptr(id);
        if (v == nullptr) {
            return 0;
        }
        v->~T();
        auto const s = idx(id) & mask;
        m_dir[idx(id) >> BlockBits]->bits[s / 64] &= ~(std::uint64_t{1} << (s % 64));
        --m_size;
        return 1;
    }
    void clear() {
        destroy_all();
    }
    void reserve(std::size_t /*n*/) {}
    auto size() const -> std::size_t {
        return m_size;
    }
    auto empty() const -> bool {
        return m_size == 0;
    }

    // iteration support
    auto at(K id) const -> T& {
        return *find_ptr(id);
    }
    auto next_at_or_after(K id, K* out) const -> bool {
        auto i = idx(id);
        for (auto p = i >> BlockBits; p < m_dir.size(); ++p, i = p << BlockBits) {
            auto* pg = m_dir[p];
            if (pg == &s_empty) {
                continue;
            }
            for (auto s = i & mask; s < slots; s = (s | 63) + 1) {
                auto b = pg->bits[s / 64] >> (s % 64);
                if (b != 0) {
                    *out = static_cast<K>((p << BlockBits) | (s + detail::ctz(b)));
                    return true;
                }
            }
        }
        return false;
    }
    auto begin() -> iterator {
        K id{};
        return next_at_or_after(K{}, &id) ? iterator(this, id, false) : end();
    }
    auto end() -> iterator {
        return iterator(this, K{}, true);
    }
    auto begin() const -> const_iterator {
        K id{};
        return next_at_or_after(K{}, &id) ? const_iterator(this, id, false) : end();
    }
    auto end() const -> const_iterator {
        return const_iterator(this, K{}, true);
    }
};

// How a packed_map block stores its values: packed by rank (a gap costs nothing, a lookup computes a
// rank), one slot per ID (a gap costs a slot), or packed until the block is half full and then one slot
// per ID, decided per block by the block's own count.
enum class layout { packed, direct, hybrid };

template <class K, class T, layout L = layout::hybrid>
class packed_map {
    static constexpr unsigned block_bits = 8;
    static constexpr std::size_t mask = (std::size_t{1} << block_bits) - 1;

    // one cache line: what a lookup needs before it can address the value
    struct alignas(64) block {
        std::uint64_t bits[4];
        T* vals;
        std::uint16_t before[4]; // entries in the words before word w
        std::uint16_t cap;
        bool direct; // vals has a slot for each of the 256 IDs
        auto count() const -> std::size_t {
            return before[3] + detail::popcount(bits[3]);
        }
        auto has(std::size_t s) const -> bool {
            return ((bits[s / 64] >> (s % 64)) & 1U) != 0;
        }
        auto rank(std::size_t s) const -> std::size_t {
            return before[s / 64] + detail::popcount(bits[s / 64] & ((std::uint64_t{1} << (s % 64)) - 1));
        }
        auto pos(std::size_t s) const -> std::size_t {
            if constexpr (L == layout::packed) {
                return rank(s);
            } else if constexpr (L == layout::direct) {
                return s;
            } else {
                return direct ? s : rank(s);
            }
        }
        template <class F>
        void each(F f) const { // f(slot in the block, position in vals), in ID order
            std::size_t r = 0;
            for (std::size_t w = 0; w < 4; ++w) {
                for (auto x = bits[w]; x != 0; x &= x - 1, ++r) {
                    auto const sl = w * 64 + detail::ctz(x);
                    f(sl, direct ? sl : r);
                }
            }
        }
    };
    static constexpr std::size_t half = 128; // layout::hybrid's switch to a slot per ID
    static_assert(sizeof(block) == 64);

    std::vector<block> m_dir;
    std::size_t m_size = 0;

    static auto idx(K id) -> std::size_t {
        return static_cast<std::size_t>(static_cast<std::make_unsigned_t<K>>(id));
    }
    // capacities grow by 2x up to 32, then by 32: a block that stops at 150 entries holds 160 slots
    static auto next_cap(std::size_t c) -> std::uint16_t {
        return static_cast<std::uint16_t>(c == 0 ? 4 : c < 32 ? c * 2 : std::min<std::size_t>(c + 32, 256));
    }

    void set_bit(block& b, std::size_t s) {
        b.bits[s / 64] |= std::uint64_t{1} << (s % 64);
        for (auto w = s / 64 + 1; w < 4; ++w) {
            ++b.before[w];
        }
        ++m_size;
    }
    // once, when a hybrid block reaches half: each value moves from its rank to its slot
    static void to_direct(block& b) {
        auto* v = static_cast<T*>(::operator new(sizeof(T) * 256));
        b.each([&](std::size_t sl, std::size_t r) {
            ::new (static_cast<void*>(v + sl)) T(std::move(b.vals[r]));
            b.vals[r].~T();
        });
        ::operator delete(b.vals);
        b.vals = v;
        b.cap = 256;
        b.direct = true;
    }

    static void free_block(block& b) {
        if (b.vals == nullptr) {
            return;
        }
        if constexpr (!std::is_trivially_destructible_v<T>) {
            b.each([&](std::size_t, std::size_t at) {
                b.vals[at].~T();
            });
        }
        ::operator delete(b.vals);
    }

public:
    using key_type = K;
    using mapped_type = T;
    using iterator = detail::iterator<packed_map, K, T>;
    using const_iterator = detail::iterator<packed_map const, K, T const>;

    packed_map() = default;
    explicit packed_map(std::size_t /*bucket_count*/) {}
    packed_map(packed_map const& o)
        : m_dir(o.m_dir)
        , m_size(o.m_size) {
        for (auto& b : m_dir) {
            if (b.vals != nullptr) {
                auto* v = static_cast<T*>(::operator new(sizeof(T) * b.cap));
                b.each([&](std::size_t, std::size_t at) {
                    ::new (static_cast<void*>(v + at)) T(b.vals[at]);
                });
                b.vals = v;
            }
        }
    }
    packed_map(packed_map&& o) noexcept
        : m_dir(std::move(o.m_dir))
        , m_size(std::exchange(o.m_size, 0)) {}
    auto operator=(packed_map o) -> packed_map& {
        std::swap(m_dir, o.m_dir);
        std::swap(m_size, o.m_size);
        return *this;
    }
    ~packed_map() {
        clear();
    }

    auto find_ptr(K id) const -> T* {
        auto const i = idx(id);
        auto const p = i >> block_bits;
        if (p >= m_dir.size()) {
            return nullptr;
        }
        auto const& b = m_dir[p];
        auto const s = i & mask;
        return b.has(s) ? b.vals + b.pos(s) : nullptr;
    }
    auto contains(K id) const -> bool {
        return find_ptr(id) != nullptr;
    }
    auto find(K id) -> iterator {
        return find_ptr(id) != nullptr ? iterator(this, id, false) : end();
    }
    auto find(K id) const -> const_iterator {
        return find_ptr(id) != nullptr ? const_iterator(this, id, false) : end();
    }

    template <class... Args>
    auto try_emplace(K id, Args&&... args) -> std::pair<iterator, bool> {
        static_assert(std::is_nothrow_move_constructible_v<T>, "the prototype shifts values with noexcept moves");
        auto const i = idx(id);
        auto const p = i >> block_bits;
        if (p >= m_dir.size()) {
            m_dir.resize(p + 1, block{});
        }
        auto& b = m_dir[p];
        auto const s = i & mask;
        if (b.has(s)) {
            return {iterator(this, id, false), false};
        }
        if (L == layout::direct && b.vals == nullptr) {
            b.vals = static_cast<T*>(::operator new(sizeof(T) * 256));
            b.cap = 256;
            b.direct = true;
        }
        if (b.direct) {
            ::new (static_cast<void*>(b.vals + s)) T(std::forward<Args>(args)...);
            set_bit(b, s);
            return {iterator(this, id, false), true};
        }
        auto const n = b.count();
        auto const r = b.rank(s);
        if (n == b.cap) {
            auto const cap = next_cap(b.cap);
            auto* v = static_cast<T*>(::operator new(sizeof(T) * cap));
            std::uninitialized_move_n(b.vals, r, v);
            ::new (static_cast<void*>(v + r)) T(std::forward<Args>(args)...);
            std::uninitialized_move_n(b.vals + r, n - r, v + r + 1);
            if (b.vals != nullptr) {
                std::destroy_n(b.vals, n);
                ::operator delete(b.vals);
            }
            b.vals = v;
            b.cap = cap;
        } else if (r == n) {
            ::new (static_cast<void*>(b.vals + n)) T(std::forward<Args>(args)...);
        } else {
            T tmp(std::forward<Args>(args)...);
            ::new (static_cast<void*>(b.vals + n)) T(std::move(b.vals[n - 1]));
            std::move_backward(b.vals + r, b.vals + n - 1, b.vals + n);
            b.vals[r] = std::move(tmp);
        }
        set_bit(b, s);
        if (L == layout::hybrid && n + 1 >= half) {
            to_direct(b);
        }
        return {iterator(this, id, false), true};
    }
    template <class V>
    auto emplace(K id, V&& v) -> std::pair<iterator, bool> {
        return try_emplace(id, std::forward<V>(v));
    }
    auto operator[](K id) -> T& {
        try_emplace(id);
        return *find_ptr(id);
    }

    auto erase(K id) -> std::size_t {
        auto const i = idx(id);
        auto const p = i >> block_bits;
        if (p >= m_dir.size() || !m_dir[p].has(i & mask)) {
            return 0;
        }
        auto& b = m_dir[p];
        auto const s = i & mask;
        if (b.direct) {
            b.vals[s].~T();
        } else {
            auto const n = b.count();
            auto const r = b.rank(s);
            std::move(b.vals + r + 1, b.vals + n, b.vals + r);
            b.vals[n - 1].~T();
        }
        b.bits[s / 64] &= ~(std::uint64_t{1} << (s % 64));
        for (auto w = s / 64 + 1; w < 4; ++w) {
            --b.before[w];
        }
        --m_size;
        return 1;
    }
    void clear() {
        for (auto& b : m_dir) {
            free_block(b);
        }
        m_dir.clear();
        m_size = 0;
    }
    void reserve(std::size_t /*n*/) {}
    auto size() const -> std::size_t {
        return m_size;
    }
    auto empty() const -> bool {
        return m_size == 0;
    }

    auto at(K id) const -> T& {
        return *find_ptr(id);
    }
    auto next_at_or_after(K id, K* out) const -> bool {
        auto i = idx(id);
        for (auto p = i >> block_bits; p < m_dir.size(); ++p, i = p << block_bits) {
            auto const& b = m_dir[p];
            for (auto s = i & mask; s <= mask; s = (s | 63) + 1) {
                auto bits = b.bits[s / 64] >> (s % 64);
                if (bits != 0) {
                    *out = static_cast<K>((p << block_bits) | (s + detail::ctz(bits)));
                    return true;
                }
            }
        }
        return false;
    }
    auto begin() -> iterator {
        K id{};
        return next_at_or_after(K{}, &id) ? iterator(this, id, false) : end();
    }
    auto end() -> iterator {
        return iterator(this, K{}, true);
    }
    auto begin() const -> const_iterator {
        K id{};
        return next_at_or_after(K{}, &id) ? const_iterator(this, id, false) : end();
    }
    auto end() const -> const_iterator {
        return const_iterator(this, K{}, true);
    }
};

// The candidate: a directory entry per page of 2^PageBits IDs holds the page's values pointer, its
// metadata pointer and whether the page is direct. A page stays packed (values in ID order, a lookup
// computes a rank from the bitmap and the counts before each word) until it holds 64 entries that
// fill half the slots up to its highest ID, then becomes direct (a slot per ID, from 0 up to a
// power of two past its highest ID, growing like a vector up to the page). A direct page that erases
// down to an eighth of its slots goes back to packed, and a page with no entries is freed. A direct
// lookup addresses the value from the directory entry alone; the bit test runs beside it.
//
// Pairs = true stores std::pair<K, T> and its iterators hand out std::pair<K, T>&, as this
// repository's map does; Pairs = false stores T and hands out a proxy {K, T&}.
//
// Tune, the knobs under measurement: OneAlloc puts a direct page's metadata and slots in one block
// (bitmap and value near each other); GrowShift is how a direct page grows (1: 2x, 2: 4x); BackDiv
// is the fraction of its slots below which a direct page goes back to packed (8: an eighth).
template <bool OneAlloc = false, unsigned GrowShift = 1, unsigned BackDiv = 8>
struct tune {
    static constexpr bool one_alloc = OneAlloc;
    static constexpr unsigned grow_shift = GrowShift;
    static constexpr unsigned back_div = BackDiv;
};

template <class K, class T, unsigned PageBits = 12, bool Pairs = false, class Tune = tune<>>
class id_map {
    static_assert(PageBits >= 6 && PageBits <= 16, "the counts before each word are 16 bit");
    static constexpr std::size_t slots = std::size_t{1} << PageBits;
    static constexpr std::size_t words = slots / 64;
    static constexpr std::size_t mask = slots - 1;
    // a packed page becomes direct once it holds this many and they fill half the slots up to its top
    static constexpr std::size_t min_direct = slots < 128 ? slots / 2 : 64;

    struct meta {
        std::uint64_t bits[words];
        std::uint16_t before[words]; // entries in the words before word w; only packed pages keep it
        std::uint32_t cap;           // values allocated: packed, entries; direct, slots from 0
        std::uint32_t top;           // highest slot ever set, plus one
        std::uint32_t live;          // entries in the page
        auto has(std::size_t s) const -> bool {
            return ((bits[s / 64] >> (s % 64)) & 1U) != 0;
        }
        auto rank(std::size_t s) const -> std::size_t {
            return before[s / 64] + detail::popcount(bits[s / 64] & ((std::uint64_t{1} << (s % 64)) - 1));
        }
        auto count() const -> std::size_t {
            return before[words - 1] + detail::popcount(bits[words - 1]);
        }
    };

public:
    using value_type = std::conditional_t<Pairs, std::pair<K, T>, T>;

private:
    using E = value_type;
    static auto value_of(E& e) -> T& {
        if constexpr (Pairs) {
            return e.second;
        } else {
            return e;
        }
    }
    struct entry {
        E* vals;
        meta* m;
        bool direct;
        auto pos(std::size_t s) const -> std::size_t {
            // a branch, not a select: a select computes the rank, and loads its counts, on every lookup
            if (__builtin_expect(direct, 1)) {
                return s;
            }
            return m->rank(s);
        }
        template <class F>
        void each(F f) const { // f(slot, position in vals), in ID order
            std::size_t r = 0;
            for (std::size_t w = 0; w < words; ++w) {
                for (auto x = m->bits[w]; x != 0; x &= x - 1, ++r) {
                    auto const sl = w * 64 + detail::ctz(x);
                    f(sl, direct ? sl : r);
                }
            }
        }
    };

    // every directory entry without a page points here: its bits stay zero, so a lookup needs no null check
    static inline meta s_empty{};

    std::vector<entry> m_dir;
    std::size_t m_size = 0;

    static auto idx(K id) -> std::size_t {
        return static_cast<std::size_t>(static_cast<std::make_unsigned_t<K>>(id));
    }
    static auto alloc(std::size_t n) -> E* {
        return static_cast<E*>(::operator new(sizeof(E) * n));
    }
    template <class... Args>
    static void construct(E* at, K id, Args&&... args) {
        if constexpr (Pairs) {
            ::new (static_cast<void*>(at))
                E(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple(std::forward<Args>(args)...));
        } else {
            (void)id;
            ::new (static_cast<void*>(at)) E(std::forward<Args>(args)...);
        }
    }
    void set_bit(entry& e, std::size_t s) {
        e.m->bits[s / 64] |= std::uint64_t{1} << (s % 64);
        e.m->top = std::max(e.m->top, static_cast<std::uint32_t>(s + 1));
        ++e.m->live;
        if (!e.direct) {
            for (auto w = s / 64 + 1; w < words; ++w) {
                ++e.m->before[w];
            }
        }
        ++m_size;
    }
    static auto direct_cap(std::size_t need) -> std::size_t {
        std::size_t c = min_direct;
        while (c < need) {
            c <<= Tune::grow_shift;
        }
        return std::min(c, slots);
    }

    // Storage of a page: packed, a meta and a values array; direct with Tune::one_alloc, one block
    // [meta | slots] whose meta pointer is the block, otherwise as packed.
    static constexpr std::size_t meta_bytes = (sizeof(meta) + 63) / 64 * 64;
    static_assert(alignof(E) <= 64, "slots follow a 64 byte aligned meta");
    struct storage {
        meta* m;
        E* vals;
    };
    // fresh direct storage with cap slots, its meta a copy of src
    static auto new_direct(meta const& src, std::size_t cap) -> storage {
        if constexpr (Tune::one_alloc) {
            auto* raw = static_cast<unsigned char*>(::operator new(meta_bytes + sizeof(E) * cap, std::align_val_t{64}));
            auto* m = ::new (static_cast<void*>(raw)) meta(src);
            return {m, reinterpret_cast<E*>(raw + meta_bytes)};
        } else {
            return {new meta(src), alloc(cap)};
        }
    }
    static void delete_storage(storage st, bool direct) {
        if (Tune::one_alloc && direct) {
            ::operator delete(static_cast<void*>(st.m), std::align_val_t{64});
        } else {
            ::operator delete(st.vals);
            delete st.m;
        }
    }
    // once, when a packed page is dense enough: each value moves from its rank to its slot
    static void to_direct(entry& e) {
        auto const cap = direct_cap(e.m->top);
        auto st = new_direct(*e.m, cap);
        e.each([&](std::size_t sl, std::size_t r) {
            ::new (static_cast<void*>(st.vals + sl)) E(std::move(e.vals[r]));
            e.vals[r].~E();
        });
        delete_storage({e.m, e.vals}, false);
        e.m = st.m;
        e.vals = st.vals;
        e.m->cap = static_cast<std::uint32_t>(cap);
        e.direct = true;
    }
    // a direct page grows like a vector, up to the page
    static void grow_direct(entry& e, std::size_t need) {
        auto const cap = direct_cap(need);
        auto st = new_direct(*e.m, cap);
        e.each([&](std::size_t sl, std::size_t) {
            ::new (static_cast<void*>(st.vals + sl)) E(std::move(e.vals[sl]));
            e.vals[sl].~E();
        });
        delete_storage({e.m, e.vals}, true);
        e.m = st.m;
        e.vals = st.vals;
        e.m->cap = static_cast<std::uint32_t>(cap);
    }
    static void free_page(entry& e) {
        if (e.m == &s_empty) {
            return;
        }
        if constexpr (!std::is_trivially_destructible_v<E>) {
            e.each([&](std::size_t, std::size_t at) {
                e.vals[at].~E();
            });
        }
        delete_storage({e.m, e.vals}, e.direct);
    }
    // a direct page erased below 1/back_div of its slots: back to packed, the counts rebuilt
    static void to_packed(entry& e) {
        auto const n = e.m->live;
        auto const cap = std::max<std::size_t>(4, n + n / 4);
        auto* m = new meta(*e.m);
        auto* v = alloc(cap);
        std::size_t r = 0;
        for (std::size_t w = 0; w < words; ++w) {
            m->before[w] = static_cast<std::uint16_t>(r);
            for (auto x = m->bits[w]; x != 0; x &= x - 1, ++r) {
                auto const sl = w * 64 + detail::ctz(x);
                ::new (static_cast<void*>(v + r)) E(std::move(e.vals[sl]));
                e.vals[sl].~E();
            }
        }
        delete_storage({e.m, e.vals}, true);
        e.m = m;
        e.vals = v;
        e.m->cap = static_cast<std::uint32_t>(cap);
        e.direct = false;
    }

public:
    using key_type = K;
    using mapped_type = T;
    using iterator = std::conditional_t<Pairs, detail::pair_iterator<id_map, K, E>, detail::iterator<id_map, K, T>>;
    using const_iterator =
        std::conditional_t<Pairs, detail::pair_iterator<id_map const, K, E const>, detail::iterator<id_map const, K, T const>>;

    id_map() = default;
    explicit id_map(std::size_t /*bucket_count*/) {}
    id_map(id_map const& o)
        : m_dir(o.m_dir)
        , m_size(o.m_size) {
        for (auto& e : m_dir) {
            if (e.m == &s_empty) {
                continue;
            }
            auto st = e.direct ? new_direct(*e.m, e.m->cap) : storage{new meta(*e.m), alloc(e.m->cap)};
            e.each([&](std::size_t, std::size_t at) {
                ::new (static_cast<void*>(st.vals + at)) E(e.vals[at]);
            });
            e.m = st.m;
            e.vals = st.vals;
        }
    }
    id_map(id_map&& o) noexcept
        : m_dir(std::move(o.m_dir))
        , m_size(std::exchange(o.m_size, 0)) {}
    auto operator=(id_map o) -> id_map& {
        std::swap(m_dir, o.m_dir);
        std::swap(m_size, o.m_size);
        return *this;
    }
    ~id_map() {
        clear();
    }

    auto find_elem(K id) const -> E* {
        auto const i = idx(id);
        auto const p = i >> PageBits;
        if (p >= m_dir.size()) {
            return nullptr;
        }
        auto const& e = m_dir[p];
        auto const s = i & mask;
        return e.m->has(s) ? e.vals + e.pos(s) : nullptr;
    }
    auto find_ptr(K id) const -> T* {
        auto* e = find_elem(id);
        return e != nullptr ? &value_of(*e) : nullptr;
    }
    auto contains(K id) const -> bool {
        return find_ptr(id) != nullptr;
    }
    auto find(K id) -> iterator {
        return find_ptr(id) != nullptr ? iterator(this, id, false) : end();
    }
    auto find(K id) const -> const_iterator {
        return find_ptr(id) != nullptr ? const_iterator(this, id, false) : end();
    }

    template <class... Args>
    auto try_emplace(K id, Args&&... args) -> std::pair<iterator, bool> {
        static_assert(std::is_nothrow_move_constructible_v<E>, "the prototype shifts values with noexcept moves");
        auto const i = idx(id);
        auto const p = i >> PageBits;
        if (p >= m_dir.size()) {
            m_dir.resize(p + 1, entry{nullptr, &s_empty, false});
        }
        auto& e = m_dir[p];
        auto const s = i & mask;
        if (e.m->has(s)) {
            return {iterator(this, id, false), false};
        }
        if (e.m == &s_empty) {
            e.m = new meta{};
        }
        if (e.direct) {
            if (s >= e.m->cap) {
                grow_direct(e, s + 1);
            }
            construct(e.vals + s, id, std::forward<Args>(args)...);
            set_bit(e, s);
            return {iterator(this, id, false), true};
        }
        auto const n = e.m->count();
        auto const r = e.m->rank(s);
        if (n == e.m->cap) {
            auto const cap = std::min<std::size_t>(slots, std::max<std::size_t>(4, n + n / 4));
            auto* v = alloc(cap);
            std::uninitialized_move_n(e.vals, r, v);
            construct(v + r, id, std::forward<Args>(args)...);
            std::uninitialized_move_n(e.vals + r, n - r, v + r + 1);
            std::destroy_n(e.vals, n);
            ::operator delete(e.vals);
            e.vals = v;
            e.m->cap = static_cast<std::uint32_t>(cap);
        } else if (r == n) {
            construct(e.vals + n, id, std::forward<Args>(args)...);
        } else {
            // shift by move-construct and destroy, so a std::pair value works without assignment
            for (auto k = n; k > r; --k) {
                ::new (static_cast<void*>(e.vals + k)) E(std::move(e.vals[k - 1]));
                e.vals[k - 1].~E();
            }
            construct(e.vals + r, id, std::forward<Args>(args)...);
        }
        set_bit(e, s);
        if (n + 1 >= min_direct && 2 * (n + 1) >= e.m->top) {
            to_direct(e);
        }
        return {iterator(this, id, false), true};
    }
    template <class V>
    auto emplace(K id, V&& v) -> std::pair<iterator, bool> {
        return try_emplace(id, std::forward<V>(v));
    }
    auto operator[](K id) -> T& {
        try_emplace(id);
        return *find_ptr(id);
    }

    auto erase(K id) -> std::size_t {
        auto const i = idx(id);
        auto const p = i >> PageBits;
        if (p >= m_dir.size() || !m_dir[p].m->has(i & mask)) {
            return 0;
        }
        auto& e = m_dir[p];
        auto const s = i & mask;
        if (e.direct) {
            e.vals[s].~E();
        } else {
            auto const n = e.m->count();
            auto const r = e.m->rank(s);
            e.vals[r].~E();
            for (auto k = r + 1; k < n; ++k) {
                ::new (static_cast<void*>(e.vals + k - 1)) E(std::move(e.vals[k]));
                e.vals[k].~E();
            }
        }
        e.m->bits[s / 64] &= ~(std::uint64_t{1} << (s % 64));
        if (!e.direct) {
            for (auto w = s / 64 + 1; w < words; ++w) {
                --e.m->before[w];
            }
        }
        --m_size;
        if (--e.m->live == 0) {
            free_page(e);
            e = entry{nullptr, &s_empty, false};
        } else if (e.direct && e.m->live * Tune::back_div < e.m->cap) {
            to_packed(e);
        }
        return 1;
    }
    void clear() {
        for (auto& e : m_dir) {
            free_page(e);
        }
        m_dir.clear();
        m_size = 0;
    }
    void reserve(std::size_t /*n*/) {}
    auto size() const -> std::size_t {
        return m_size;
    }
    auto empty() const -> bool {
        return m_size == 0;
    }

    auto at(K id) const -> T& {
        return *find_ptr(id);
    }
    auto next_at_or_after(K id, K* out) const -> bool {
        auto i = idx(id);
        for (auto p = i >> PageBits; p < m_dir.size(); ++p, i = p << PageBits) {
            auto const* m = m_dir[p].m;
            for (auto s = i & mask; s < slots; s = (s | 63) + 1) {
                auto b = m->bits[s / 64] >> (s % 64);
                if (b != 0) {
                    *out = static_cast<K>((p << PageBits) | (s + detail::ctz(b)));
                    return true;
                }
            }
        }
        return false;
    }
    auto begin() -> iterator {
        K id{};
        return next_at_or_after(K{}, &id) ? iterator(this, id, false) : end();
    }
    auto end() -> iterator {
        return iterator(this, K{}, true);
    }
    auto begin() const -> const_iterator {
        K id{};
        return next_at_or_after(K{}, &id) ? const_iterator(this, id, false) : end();
    }
    auto end() const -> const_iterator {
        return const_iterator(this, K{}, true);
    }
};

} // namespace idm
