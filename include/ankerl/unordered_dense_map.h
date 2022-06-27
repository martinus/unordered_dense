///////////////////////// ankerl::unordered_dense_map /////////////////////////

// A fast & densely stored hashmap based on robin-hood backward shift deletion.
// Version 0.0.1
// https://github.com/martinus/unordered_dense_map
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2022 Martin Leitner-Ankerl <martin.ankerl@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef ANKERL_UNORDERED_DENSE_MAP_H
#define ANKERL_UNORDERED_DENSE_MAP_H

// see https://semver.org/spec/v2.0.0.html
#define ANKERL_UNORDERED_DENSE_MAP_VERSION_MAJOR 0 // incompatible API changes
#define ANKERL_UNORDERED_DENSE_MAP_VERSION_MINOR 0 // add functionality in a backwards compatible manner
#define ANKERL_UNORDERED_DENSE_MAP_VERSION_PATCH 1 // backwards compatible bug fixes

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_MSC_VER) && defined(_M_X64)
#    include <intrin.h>
#    pragma intrinsic(_umul128)
#endif

// likely and unlikely macros
#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#    define ANKERL_UNORDERED_DENSE_MAP_LIKELY(x) __builtin_expect(x, 1)
#    define ANKERL_UNORDERED_DENSE_MAP_UNLIKELY(x) __builtin_expect(x, 0)
#else
#    define ANKERL_UNORDERED_DENSE_MAP_LIKELY(x) (x)
#    define ANKERL_UNORDERED_DENSE_MAP_UNLIKELY(x) (x)
#endif

namespace ankerl {

// This is a stripped-down implementation of wyhash: https://github.com/wangyi-fudan/wyhash
// Notably this removes big-endian support (because different values on different machines don't matter),
// hardcodes seed and the secret, reformattes the code, and clang-tidy fixes.
namespace detail::wyhash {

static inline void mum(uint64_t* a, uint64_t* b) {
#if defined(__SIZEOF_INT128__)
    __uint128_t r = *a;
    r *= *b;
    *a = static_cast<uint64_t>(r);
    *b = static_cast<uint64_t>(r >> 64U);
#elif defined(_MSC_VER) && defined(_M_X64)
    *a = _umul128(*a, *b, b);
#else
    uint64_t ha = *a >> 32U;
    uint64_t hb = *b >> 32U;
    uint64_t la = static_cast<uint32_t>(*a);
    uint64_t lb = static_cast<uint32_t>(*b);
    uint64_t hi{};
    uint64_t lo{};
    uint64_t rh = ha * hb;
    uint64_t rm0 = ha * lb;
    uint64_t rm1 = hb * la;
    uint64_t rl = la * lb;
    uint64_t t = rl + (rm0 << 32U);
    auto c = static_cast<uint64_t>(t < rl);
    lo = t + (rm1 << 32U);
    c += static_cast<uint64_t>(lo < t);
    hi = rh + (rm0 >> 32U) + (rm1 >> 32U) + c;
    *a = lo;
    *b = hi;
#endif
}

// multiply and xor mix function, aka MUM
[[nodiscard]] static inline auto mix(uint64_t a, uint64_t b) -> uint64_t {
    mum(&a, &b);
    return a ^ b;
}

// read functions. WARNING: we don't care about endianness, so results are different on big endian!
[[nodiscard]] static inline auto r8(const uint8_t* p) -> uint64_t {
    uint64_t v{};
    std::memcpy(&v, p, 8);
    return v;
}

[[nodiscard]] static inline auto r4(const uint8_t* p) -> uint64_t {
    uint32_t v{};
    std::memcpy(&v, p, 4);
    return v;
}

// reads 1, 2, or 3 bytes
[[nodiscard]] static inline auto r3(const uint8_t* p, size_t k) -> uint64_t {
    return (static_cast<uint64_t>(p[0]) << 16U) | (static_cast<uint64_t>(p[k >> 1U]) << 8U) | p[k - 1];
}

[[nodiscard]] static inline auto hash(const void* key, size_t len) -> uint64_t {
    static constexpr auto secret =
        std::array{0xa0761d6478bd642fULL, 0xe7037ed1a0b428dbULL, 0x8ebc6af09c88c6e3ULL, 0x589965cc75374cc3ULL};

    auto const* p = static_cast<const uint8_t*>(key);
    uint64_t seed = secret[0];
    uint64_t a{};
    uint64_t b{};
    if (ANKERL_UNORDERED_DENSE_MAP_LIKELY(len <= 16)) {
        if (ANKERL_UNORDERED_DENSE_MAP_LIKELY(len >= 4)) {
            a = (r4(p) << 32U) | r4(p + ((len >> 3U) << 2U));
            b = (r4(p + len - 4) << 32U) | r4(p + len - 4 - ((len >> 3U) << 2U));
        } else if (ANKERL_UNORDERED_DENSE_MAP_LIKELY(len > 0)) {
            a = r3(p, len);
            b = 0;
        } else {
            a = 0;
            b = 0;
        }
    } else {
        size_t i = len;
        if (ANKERL_UNORDERED_DENSE_MAP_UNLIKELY(i > 48)) {
            uint64_t see1 = seed;
            uint64_t see2 = seed;
            do {
                seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
                see1 = mix(r8(p + 16) ^ secret[2], r8(p + 24) ^ see1);
                see2 = mix(r8(p + 32) ^ secret[3], r8(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (ANKERL_UNORDERED_DENSE_MAP_LIKELY(i > 48));
            seed ^= see1 ^ see2;
        }
        while (ANKERL_UNORDERED_DENSE_MAP_UNLIKELY(i > 16)) {
            seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }
        a = r8(p + i - 16);
        b = r8(p + i - 8);
    }

    return mix(secret[1] ^ len, mix(a ^ secret[1], b ^ seed));
}

} // namespace detail::wyhash

namespace detail {

struct nonesuch {};

template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
struct detector {
    using value_t = std::false_type;
    using type = Default;
};

template <class Default, template <class...> class Op, class... Args>
struct detector<Default, std::void_t<Op<Args...>>, Op, Args...> {
    using value_t = std::true_type;
    using type = Op<Args...>;
};

} // namespace detail

template <template <class...> class Op, class... Args>
using is_detected = typename detail::detector<detail::nonesuch, void, Op, Args...>::value_t;

template <template <class...> class Op, class... Args>
constexpr bool is_detected_v = is_detected<Op, Args...>::value;

template <typename T, typename Enable = void>
struct hash : public std::hash<T> {
    auto operator()(T const& obj) const noexcept(noexcept(std::declval<std::hash<T>>().operator()(std::declval<T const&>())))
        -> size_t {
        return std::hash<T>::operator()(obj);
    }
};

template <typename T>
using detect_avalanching = typename T::is_avalanching;

template <typename CharT>
struct hash<std::basic_string<CharT>> {
    using is_avalanching = void;
    auto operator()(std::basic_string<CharT> const& str) const noexcept -> size_t {
        return detail::wyhash::hash(str.data(), sizeof(CharT) * str.size());
    }
};

template <typename CharT>
struct hash<std::basic_string_view<CharT>> {
    using is_avalanching = void;
    auto operator()(std::basic_string_view<CharT> const& sv) const noexcept -> size_t {
        return detail::wyhash::hash(sv.data(), sizeof(CharT) * sv.size());
    }
};

/**
 * @brief
 *
 * @tparam Key
 * @tparam T
 * @tparam Hash
 * @tparam Pred
 */
template <class Key,
          class T,
          class Hash = hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<Key, T>>>
class unordered_dense_map {
    struct Bucket;
    using ValueContainer = std::vector<std::pair<Key, T>, Allocator>;
    using BucketAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<Bucket>;
    using BucketAllocTraits = std::allocator_traits<BucketAlloc>;

    // 1 byte of fingerprint
    static constexpr uint32_t BUCKET_DIST_INC = 1U << 8U;
    static constexpr uint32_t BUCKET_FINGERPRINT_MASK = BUCKET_DIST_INC - 1;
    static constexpr uint8_t INITIAL_SHIFTS = 64 - 3;

    struct Bucket {
        /**
         * Upper 3 byte encode the distance to the original bucket. 0 means empty, 1 means here, ...
         * Lower 1 byte encodes a fingerprint; 8 bits from the hash.
         */
        uint32_t dist_and_fingerprint{};

        /**
         * Index into the m_values vector.
         */
        uint32_t value_idx{};
    };
    static_assert(std::is_trivially_destructible_v<Bucket>, "assert there's no need to call destructor / std::destroy");
    static_assert(std::is_trivially_copyable_v<Bucket>, "assert we can just memset / memcpy");

    /**
     * Contains all the key-value pairs in one densely stored container. No holes.
     */
    ValueContainer m_values{};
    Bucket* m_buckets_start = nullptr;
    Bucket* m_buckets_end = nullptr;
    uint32_t m_max_bucket_capacity = 0;
    float m_max_load_factor = 0.8;
    Hash m_hash{};
    KeyEqual m_equal{};
    uint8_t m_shifts = INITIAL_SHIFTS;

    [[nodiscard]] auto next(Bucket const* bucket) const -> Bucket const* {
        if (ANKERL_UNORDERED_DENSE_MAP_UNLIKELY(++bucket == m_buckets_end)) {
            return m_buckets_start;
        }
        return bucket;
    }

    [[nodiscard]] auto next(Bucket* bucket) -> Bucket* {
        if (ANKERL_UNORDERED_DENSE_MAP_UNLIKELY(++bucket == m_buckets_end)) {
            return m_buckets_start;
        }
        return bucket;
    }

    [[nodiscard]] constexpr auto mixed_hash(Key const& key) const -> uint64_t {
        if constexpr (is_detected_v<detect_avalanching, Hash>) {
            return m_hash(key);
        }
        // See https://godbolt.org/z/Pvo5Kf4Gx, mix() compiles to
        //   movabs  rcx, -7046029254386353131
        //   mov     rax, rcx
        //   mul     rdi
        //   xor     rax, rdx
        //   ret
        return detail::wyhash::mix(m_hash(key), UINT64_C(0x9E3779B97F4A7C15));
    }

    [[nodiscard]] constexpr auto dist_and_fingerprint_from_hash(uint64_t hash) const -> uint32_t {
        return BUCKET_DIST_INC | (hash & BUCKET_FINGERPRINT_MASK);
    }

    [[nodiscard]] constexpr auto bucket_from_hash(uint64_t hash) const -> Bucket const* {
        return m_buckets_start + (hash >> m_shifts);
    }

    [[nodiscard]] constexpr auto bucket_from_hash(uint64_t hash) -> Bucket* {
        return m_buckets_start + (hash >> m_shifts);
    }

    auto next_while_less(Key const& key) -> std::pair<uint32_t, Bucket*> {
        auto const& pair = std::as_const(*this).next_while_less(key);
        return {pair.first, const_cast<Bucket*>(pair.second)}; // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }

    auto next_while_less(Key const& key) const -> std::pair<uint32_t, Bucket const*> {
        auto hash = mixed_hash(key);
        auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
        auto const* bucket = bucket_from_hash(hash);

        while (dist_and_fingerprint < bucket->dist_and_fingerprint) {
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);
        }
        return {dist_and_fingerprint, bucket};
    }

    void place_and_shift_up(Bucket bucket, Bucket* place) {
        while (0 != place->dist_and_fingerprint) {
            bucket = std::exchange(*place, bucket);
            bucket.dist_and_fingerprint += BUCKET_DIST_INC;
            place = next(place);
        }
        *place = bucket;
    }

    [[nodiscard]] static constexpr auto calc_num_buckets(uint8_t shifts) -> uint64_t {
        return UINT64_C(1) << (64U - shifts);
    }

    // assumes m_values has data, m_buckets_start=m_buckets_end=nullptr, m_shifts is INITIAL_SHIFTS
    void init_from_values() {
        if (!empty()) {
            while (static_cast<uint64_t>(calc_num_buckets(m_shifts) * max_load_factor()) < size()) {
                --m_shifts;
            }
            allocate_and_clear_buckets();
            fill_buckets_from_values();
        }
    }

    /**
     * True when no element can be added any more without increasing the size
     */
    [[nodiscard]] auto is_full() const -> bool {
        return size() >= m_max_bucket_capacity;
    }

    void deallocate_buckets() {
        auto bucket_alloc = BucketAlloc(m_values.get_allocator());

        // we can first deallocate the bucket array and then allocate the larger one. This is really nice
        // because it means there's no memory spike. (Note there is still a spike when the std::vector resizes)
        BucketAllocTraits::deallocate(bucket_alloc, m_buckets_start, m_buckets_end - m_buckets_start);
        m_buckets_start = nullptr;
        m_buckets_end = nullptr;
        m_max_bucket_capacity = 0;
    }

    void allocate_and_clear_buckets() {
        auto bucket_alloc = BucketAlloc(m_values.get_allocator());
        auto num_buckets = UINT64_C(1) << (64U - m_shifts);
        m_buckets_start = BucketAllocTraits::allocate(bucket_alloc, num_buckets);
        m_buckets_end = m_buckets_start + num_buckets;
        m_max_bucket_capacity = static_cast<uint64_t>(num_buckets * max_load_factor());
        clear_buckets();
    }

    void clear_buckets() {
        std::memset(m_buckets_start, 0, sizeof(Bucket) * (m_buckets_end - m_buckets_start));
    }

    void fill_buckets_from_values() {
        for (uint32_t value_idx = 0, end_idx = m_values.size(); value_idx < end_idx; ++value_idx) {
            auto const& key = m_values[value_idx].first;
            auto [dist_and_fingerprint, bucket] = next_while_less(key);

            // we know for certain that key has not yet been inserted, so no need to check it.
            place_and_shift_up({dist_and_fingerprint, value_idx}, bucket);
        }
    }

    void increase_size() {
        --m_shifts;
        deallocate_buckets();
        allocate_and_clear_buckets();
        fill_buckets_from_values();
    }

    void do_erase(Bucket* bucket) {
        auto const value_idx_to_remove = bucket->value_idx;

        // shift down until either empty or an element with correct spot is found
        auto* next_bucket = next(bucket);
        while (next_bucket->dist_and_fingerprint >= BUCKET_DIST_INC * 2) {
            *bucket = {next_bucket->dist_and_fingerprint - BUCKET_DIST_INC, next_bucket->value_idx};
            bucket = std::exchange(next_bucket, next(next_bucket));
        }
        *bucket = {};

        // update m_values
        if (value_idx_to_remove != m_values.size() - 1) {
            // no luck, we'll have to replace the value with the last one and update the index accordingly
            auto& val = m_values[value_idx_to_remove];
            val = std::move(m_values.back());

            // update the values_idx of the moved entry. No need to play the info game, just look until we find the values_idx
            // TODO don't duplicate code
            auto mh = mixed_hash(val.first);
            bucket = bucket_from_hash(mh);

            auto const values_idx_back = static_cast<uint32_t>(m_values.size() - 1);
            while (values_idx_back != bucket->value_idx) {
                bucket = next(bucket);
            }
            bucket->value_idx = value_idx_to_remove;
        }
        m_values.pop_back();
    }

public:
    using value_type = std::pair<Key, T>;
    using key_type = Key;
    using mapped_type = T;
    using size_type = size_t;
    using const_iterator = typename ValueContainer::const_iterator;
    using iterator = typename ValueContainer::iterator;

    unordered_dense_map()
        : unordered_dense_map(0) {}

    explicit unordered_dense_map(size_t /*bucket_count*/,
                                 Hash const& hash = Hash(),
                                 KeyEqual const& equal = KeyEqual(),
                                 Allocator const& alloc = Allocator())
        : m_values(alloc)
        , m_hash(hash)
        , m_equal(equal) {}

    unordered_dense_map(size_t bucket_count, Allocator const& alloc)
        : unordered_dense_map(bucket_count, Hash(), KeyEqual(), alloc) {}

    unordered_dense_map(size_t bucket_count, Hash const& hash, Allocator const& alloc)
        : unordered_dense_map(bucket_count, hash, KeyEqual(), alloc) {}

    explicit unordered_dense_map(Allocator const& alloc)
        : unordered_dense_map(0, Hash(), KeyEqual(), alloc) {}

    template <class InputIt>
    unordered_dense_map(InputIt first,
                        InputIt last,
                        size_type bucket_count = 0,
                        Hash const& hash = Hash(),
                        KeyEqual const& equal = KeyEqual(),
                        Allocator const& alloc = Allocator())
        : unordered_dense_map(bucket_count, hash, equal, alloc) {
        insert(first, last);
    }

    template <class InputIt>
    unordered_dense_map(InputIt first, InputIt last, size_type bucket_count, Allocator const& alloc)
        : unordered_dense_map(first, last, bucket_count, Hash(), KeyEqual(), alloc) {}

    template <class InputIt>
    unordered_dense_map(InputIt first, InputIt last, size_type bucket_count, Hash const& hash, Allocator const& alloc)
        : unordered_dense_map(first, last, bucket_count, hash, KeyEqual(), alloc) {}

    unordered_dense_map(unordered_dense_map const& other)
        : unordered_dense_map(other, other.m_values.get_allocator()) {}

    unordered_dense_map(unordered_dense_map const& other, Allocator const& alloc)
        : m_values(other.m_values, alloc)
        , m_max_load_factor(other.m_max_load_factor)
        , m_hash(other.m_hash)
        , m_equal(other.m_equal) {
        init_from_values();
    }

    unordered_dense_map(unordered_dense_map&& other) noexcept
        : unordered_dense_map(std::move(other), other.m_values.get_allocator()) {}

    unordered_dense_map(unordered_dense_map&& other, Allocator const& alloc) noexcept
        : m_values(std::move(other.m_values), alloc)
        , m_buckets_start(other.m_buckets_start)
        , m_buckets_end(other.m_buckets_end)
        , m_max_bucket_capacity(other.m_max_bucket_capacity)
        , m_max_load_factor(other.m_max_load_factor)
        , m_hash(std::move(other.m_hash))
        , m_equal(std::move(other.m_equal))
        , m_shifts(other.m_shifts) {
        other.m_buckets_start = nullptr;
    }

    unordered_dense_map(std::initializer_list<value_type> ilist,
                        size_t bucket_count = 0,
                        Hash const& hash = Hash(),
                        KeyEqual const& equal = KeyEqual(),
                        Allocator const& alloc = Allocator())
        : unordered_dense_map(bucket_count, hash, equal, alloc) {
        insert(ilist);
    }

    unordered_dense_map(std::initializer_list<value_type> ilist, size_type bucket_count, const Allocator& alloc)
        : unordered_dense_map(ilist, bucket_count, Hash(), KeyEqual(), alloc) {}

    unordered_dense_map(std::initializer_list<value_type> init,
                        size_type bucket_count,
                        Hash const& hash,
                        Allocator const& alloc)
        : unordered_dense_map(init, bucket_count, hash, KeyEqual(), alloc) {}

    auto operator=(unordered_dense_map&& other) noexcept -> unordered_dense_map& {
        if (&other != this) {
            m_values = std::move(other.m_values);
            m_buckets_start = std::exchange(other.m_buckets_start, nullptr);
            m_buckets_end = other.m_buckets_end;
            m_max_bucket_capacity = other.m_max_bucket_capacity;
            m_max_load_factor = other.m_max_load_factor;
            m_hash = std::move(other.m_hash);
            m_equal = std::move(other.m_equal);
            m_shifts = other.m_shifts;
            other.m_buckets_start = nullptr;
        }
        return *this;
    }

    auto operator=(unordered_dense_map const& other) -> unordered_dense_map& {
        if (&other != this) {
            deallocate_buckets(); // deallocate before m_values is set (might have another allocator)
            m_values = other.m_values;
            m_max_load_factor = other.m_max_load_factor;
            m_hash = other.m_hash;
            m_equal = other.m_equal;
            m_shifts = INITIAL_SHIFTS;
            init_from_values();
        }
        return *this;
    }

    ~unordered_dense_map() {
        auto bucket_alloc = BucketAlloc(m_values.get_allocator());
        BucketAllocTraits::deallocate(bucket_alloc, m_buckets_start, m_buckets_end - m_buckets_start);
    }

    void swap(unordered_dense_map& other) {
        using std::swap;
        swap(other, *this);
    }

    void clear() {
        m_values.clear();
        clear_buckets();
    }

    auto cend() const -> const_iterator {
        return m_values.cend();
    }
    auto end() const -> const_iterator {
        return m_values.end();
    }
    auto end() -> iterator {
        return m_values.end();
    }

    auto cbegin() const -> const_iterator {
        return m_values.cbegin();
    }
    auto begin() const -> const_iterator {
        return m_values.begin();
    }
    auto begin() -> iterator {
        return m_values.begin();
    }

    template <typename K>
    auto find(K const& key) -> iterator {
        if (empty()) {
            return end();
        }
        auto mh = mixed_hash(key);
        auto dist_and_fingerprint = dist_and_fingerprint_from_hash(mh);
        auto const* bucket = bucket_from_hash(mh);

        // do-while is faster than while. The inner loop is unrolled 4 times, which in my benchmark produced the fastest code
        do {
            if (dist_and_fingerprint == bucket->dist_and_fingerprint && m_equal(key, m_values[bucket->value_idx].first)) {
                return begin() + bucket->value_idx;
            }
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);

            if (dist_and_fingerprint == bucket->dist_and_fingerprint && m_equal(key, m_values[bucket->value_idx].first)) {
                return begin() + bucket->value_idx;
            }
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);

            if (dist_and_fingerprint == bucket->dist_and_fingerprint && m_equal(key, m_values[bucket->value_idx].first)) {
                return begin() + bucket->value_idx;
            }
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);
        } while (dist_and_fingerprint <= bucket->dist_and_fingerprint);
        return end();
    }

    template <typename K>
    auto find(K const& key) const -> const_iterator {
        return const_cast<unordered_dense_map*>(this)->find(key); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }

    template <typename K>
    auto count(K const& key) const -> size_t {
        return find(key) == end() ? 0 : 1;
    }

    auto at(key_type const& key) -> T& {
        if (auto it = find(key); end() != it) {
            return it->second;
        }
        throw std::out_of_range("ankerl::unordered_dense_map::at(): key not found");
    }

    auto at(key_type const& key) const -> T const& {
        return const_cast<unordered_dense_map*>(this)->at(key); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }

    auto operator[](Key&& key) -> T& {
        return try_emplace(std::move(key)).first->second;
    }

    auto operator[](Key const& key) -> T& {
        return try_emplace(key).first->second;
    }

    auto insert(value_type const& value) -> std::pair<iterator, bool> {
        return try_emplace(value.first, value.second);
    }

    template <class K, class M>
    auto insert_or_assign(K&& key, M&& mapped) -> std::pair<iterator, bool> {
        auto it_isinserted = try_emplace(std::forward<K>(key), std::forward<M>(mapped));
        if (!it_isinserted.second) {
            it_isinserted.first->second = std::forward<M>(mapped);
        }
        return it_isinserted;
    }

    template <class K, class M>
    auto insert_or_assign(const_iterator /*hint*/, K&& key, M&& mapped) -> iterator {
        return insert_or_assign(std::forward<K>(key), std::forward<M>(mapped)).first;
    }

    template <class... Args>
    auto emplace(Args&&... args) -> std::pair<iterator, bool> {
        if (is_full()) {
            increase_size();
        }

        // first emplace the object back. If the key is already there, pop it.
        m_values.emplace_back(std::forward<Args>(args)...);

        auto& val = m_values.back();
        auto hash = mixed_hash(val.first);
        auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
        auto* bucket = bucket_from_hash(hash);

        while (dist_and_fingerprint <= bucket->dist_and_fingerprint) {
            if (dist_and_fingerprint == bucket->dist_and_fingerprint &&
                m_equal(val.first, m_values[bucket->value_idx].first)) {
                // value was already there, so get rid of it.
                m_values.pop_back();
                return {begin() + bucket->value_idx, false};
            }
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);
        }

        // value is new, place the bucket and shift up until we find an empty spot
        uint32_t value_idx = static_cast<uint32_t>(m_values.size()) - 1;
        place_and_shift_up({dist_and_fingerprint, value_idx}, bucket);

        return {begin() + value_idx, true};
    }

    template <typename K, typename... Args>
    auto try_emplace(K&& key, Args&&... args) -> std::pair<iterator, bool> {
        if (is_full()) {
            increase_size();
        }

        auto hash = mixed_hash(key);
        auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
        auto* bucket = bucket_from_hash(hash);

        while (dist_and_fingerprint <= bucket->dist_and_fingerprint) {
            if (dist_and_fingerprint == bucket->dist_and_fingerprint && m_equal(key, m_values[bucket->value_idx].first)) {
                return {begin() + bucket->value_idx, false};
            }
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);
        }

        // emplace the new value. If that throws an exception, no harm done; index is still in a valid state
        m_values.emplace_back(std::piecewise_construct,
                              std::forward_as_tuple(std::forward<K>(key)),
                              std::forward_as_tuple(std::forward<Args>(args)...));

        // place element and shift up until we find an empty spot
        uint32_t value_idx = static_cast<uint32_t>(m_values.size()) - 1;
        place_and_shift_up({dist_and_fingerprint, value_idx}, bucket);

        return {begin() + value_idx, true};
    }

    template <typename InputIt>
    void insert(InputIt first, InputIt last) {
        while (first != last) {
            emplace(*first);
            ++first;
        }
    }

    void insert(std::initializer_list<value_type> ilist) {
        for (auto&& vt : ilist) {
            emplace(std::move(vt));
        }
    }

    auto erase(const_iterator it) -> iterator {
        return erase(begin() + (it - cbegin()));
    }

    auto erase(iterator it) -> iterator {
        auto hash = mixed_hash(it->first);
        auto* bucket = bucket_from_hash(hash);

        auto const value_idx_to_remove = static_cast<uint32_t>(it - cbegin());
        while (bucket->value_idx != value_idx_to_remove) {
            bucket = next(bucket);
        }

        do_erase(bucket);
        return it;
    }

    auto erase(Key const& key) -> size_t {
        auto [dist_and_fingerprint, bucket] = next_while_less(key);

        while (dist_and_fingerprint == bucket->dist_and_fingerprint && !m_equal(key, m_values[bucket->value_idx].first)) {
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);
        }

        if (dist_and_fingerprint != bucket->dist_and_fingerprint) {
            return 0;
        }
        do_erase(bucket);
        return 1;
    }

    [[nodiscard]] auto empty() const -> bool {
        return m_values.empty();
    }

    [[nodiscard]] auto size() const -> size_t {
        return m_values.size();
    }

    [[nodiscard]] auto max_load_factor() const -> float {
        return m_max_load_factor;
    }

    void max_load_factor(float ml) {
        m_max_load_factor = ml;
        m_max_bucket_capacity = static_cast<uint64_t>((m_buckets_end - m_buckets_start) * max_load_factor());
    }

    friend auto operator==(unordered_dense_map const& a, unordered_dense_map const& b) -> bool {
        if (&a == &b) {
            return true;
        }
        if (a.size() != b.size()) {
            return false;
        }
        for (auto const& b_entry : b) {
            if (auto it = a.find(b_entry.first); a.end() == it || b_entry.second != it->second) {
                return false;
            }
        }
        return true;
    }

    friend auto operator!=(unordered_dense_map const& a, unordered_dense_map const& b) -> bool {
        return !(a == b);
    }
};

} // namespace ankerl

#endif
