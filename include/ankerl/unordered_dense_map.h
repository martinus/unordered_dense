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

static inline void wymum(uint64_t* a, uint64_t* b) {
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
static inline auto wymix(uint64_t a, uint64_t b) -> uint64_t {
    wymum(&a, &b);
    return a ^ b;
}

// read functions. WARNING: we don't care about endianness, so results are different on big endian!
static inline auto wyr8(const uint8_t* p) -> uint64_t {
    uint64_t v{};
    std::memcpy(&v, p, 8);
    return v;
}
static inline auto wyr4(const uint8_t* p) -> uint64_t {
    uint32_t v{};
    std::memcpy(&v, p, 4);
    return v;
}

static inline auto wyr3(const uint8_t* p, size_t k) -> uint64_t {
    return ((static_cast<uint64_t>(p[0])) << 16U) | ((static_cast<uint64_t>(p[k >> 1U])) << 8U) | p[k - 1];
}

static inline auto wyhash(const void* key, size_t len) -> uint64_t {
    uint64_t seed = 0;
    constexpr auto secret =
        std::array{0xa0761d6478bd642fULL, 0xe7037ed1a0b428dbULL, 0x8ebc6af09c88c6e3ULL, 0x589965cc75374cc3ULL};

    auto const* p = static_cast<const uint8_t*>(key);
    seed ^= secret[0];
    uint64_t a{};
    uint64_t b{};
    if (ANKERL_UNORDERED_DENSE_MAP_LIKELY(len <= 16)) {
        if (ANKERL_UNORDERED_DENSE_MAP_LIKELY(len >= 4)) {
            a = (wyr4(p) << 32U) | wyr4(p + ((len >> 3U) << 2U));
            b = (wyr4(p + len - 4) << 32U) | wyr4(p + len - 4 - ((len >> 3U) << 2U));
        } else if (ANKERL_UNORDERED_DENSE_MAP_LIKELY(len > 0)) {
            a = wyr3(p, len);
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
                seed = wymix(wyr8(p) ^ secret[1], wyr8(p + 8) ^ seed);
                see1 = wymix(wyr8(p + 16) ^ secret[2], wyr8(p + 24) ^ see1);
                see2 = wymix(wyr8(p + 32) ^ secret[3], wyr8(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (ANKERL_UNORDERED_DENSE_MAP_LIKELY(i > 48));
            seed ^= see1 ^ see2;
        }
        while (ANKERL_UNORDERED_DENSE_MAP_UNLIKELY(i > 16)) {
            seed = wymix(wyr8(p) ^ secret[1], wyr8(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }
        a = wyr8(p + i - 16);
        b = wyr8(p + i - 8);
    }

    return wymix(secret[1] ^ len, wymix(a ^ secret[1], b ^ seed));
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
        return detail::wyhash::wyhash(str.data(), sizeof(CharT) * str.size());
    }
};

template <typename CharT>
struct hash<std::basic_string_view<CharT>> {
    using is_avalanching = void;
    auto operator()(std::basic_string_view<CharT> const& sv) const noexcept -> size_t {
        return detail::wyhash::wyhash(sv.data(), sizeof(CharT) * sv.size());
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
template <class Key, class T, class Hash = hash<Key>, class Pred = std::equal_to<Key>>
class unordered_dense_map {
    using ValueContainer = std::vector<std::pair<Key, T>>;

public:
    using value_type = std::pair<Key, T>;
    using key_type = Key;
    using size_type = size_t;
    using const_iterator = typename ValueContainer::const_iterator;
    using iterator = typename ValueContainer::iterator;

private:
    static constexpr uint32_t BUCKET_DIST_INC = 256;

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

    /**
     * Contains all the key-value pairs in one densely stored container. No holes.
     */
    ValueContainer m_values{};

    Bucket* m_buckets_start = nullptr;
    Bucket* m_buckets_end = nullptr;
    uint32_t m_max_bucket_capacity = 0;
    Hash m_hash{};
    Pred m_equals{};
    uint8_t m_shifts{61};

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
        return detail::wyhash::wymix(m_hash(key), 0xe7037ed1a0b428dbULL);
    }

    [[nodiscard]] constexpr auto dist_and_fingerprint_from_hash(uint64_t hash) const -> uint32_t {
        return BUCKET_DIST_INC | (static_cast<uint32_t>(hash) & UINT64_C(0xFF));
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

public:
    unordered_dense_map() = default;

    unordered_dense_map(unordered_dense_map&& other) noexcept
        : m_values(std::move(other.m_values))
        , m_buckets_start(other.m_buckets_start)
        , m_buckets_end(other.m_buckets_end)
        , m_max_bucket_capacity(other.m_max_bucket_capacity)
        , m_hash(std::move(other.m_hash))
        , m_equals(std::move(other.m_equals))
        , m_shifts(other.m_shifts) {
        other.m_buckets_start = nullptr;
    }

    auto operator=(unordered_dense_map&& other) noexcept -> unordered_dense_map& {
        if (&other != this) {
            m_values = std::move(other.m_values);
            m_buckets_start = std::exchange(other.m_buckets_start, nullptr);
            m_buckets_end = other.m_buckets_end;
            m_max_bucket_capacity = other.m_max_bucket_capacity;
            m_hash = std::move(other.m_hash);
            m_equals = std::move(other.m_equals);
            m_shifts = other.m_shifts;
        }
        return *this;
    }

    [[nodiscard]] static constexpr auto calc_num_buckets(uint8_t shifts) -> uint64_t {
        return UINT64_C(1) << (64U - shifts);
    }

    unordered_dense_map(unordered_dense_map const& other)
        : m_values(other.m_values) {

        if (!other.empty()) {
            while (static_cast<uint64_t>(calc_num_buckets(m_shifts) * max_load_factor()) < other.size()) {
                --m_shifts;
            }

            allocate_new_bucket_array();
            fill_bucket_array_from_values();
        }
    }

    auto operator=(unordered_dense_map const& other) -> unordered_dense_map& {
        if (&other != this) {
            auto shifts = m_shifts;
            while (static_cast<uint64_t>(calc_num_buckets(shifts) * max_load_factor()) < other.size()) {
                --shifts;
            }

            if (shifts != m_shifts) {
                m_shifts = shifts;
                allocate_new_bucket_array();
            } else {
                std::memset(m_buckets_start, 0, sizeof(Bucket) * calc_num_buckets(m_shifts));
            }
            fill_bucket_array_from_values();
        }
        return *this;
    }

    ~unordered_dense_map() {
        delete[] m_buckets_start;
    }

    void clear() {
        m_values.clear();
        auto num_buckets = UINT64_C(1) << (64U - m_shifts);
        std::memset(m_buckets_start, 0, sizeof(Bucket) * num_buckets);
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
    auto find(K const& key) const -> const_iterator {
        auto mh = mixed_hash(key);
        auto dist_and_fingerprint = dist_and_fingerprint_from_hash(mh);
        auto const* bucket = bucket_from_hash(mh);

        // do-while is faster than while. The inner loop is unrolled 4 times, which in my benchmark produced the fastest code
        do {
            if (dist_and_fingerprint == bucket->dist_and_fingerprint && m_equals(key, m_values[bucket->value_idx].first)) {
                return begin() + bucket->value_idx;
            }
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);

            if (dist_and_fingerprint == bucket->dist_and_fingerprint && m_equals(key, m_values[bucket->value_idx].first)) {
                return begin() + bucket->value_idx;
            }
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);

            if (dist_and_fingerprint == bucket->dist_and_fingerprint && m_equals(key, m_values[bucket->value_idx].first)) {
                return begin() + bucket->value_idx;
            }
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);
        } while (dist_and_fingerprint <= bucket->dist_and_fingerprint);
        return end();
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
                m_equals(val.first, m_values[bucket->value_idx].first)) {
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
            if (dist_and_fingerprint == bucket->dist_and_fingerprint && m_equals(key, m_values[bucket->value_idx].first)) {
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

    /**
     * True when no element can be added any more without increasing the size
     */
    [[nodiscard]] auto is_full() const -> bool {
        return size() == m_max_bucket_capacity;
    }

    void allocate_new_bucket_array() {
        delete[] m_buckets_start;

        auto num_buckets = UINT64_C(1) << (64U - m_shifts);
        m_max_bucket_capacity = static_cast<uint64_t>(num_buckets * max_load_factor());
        m_buckets_start = new Bucket[num_buckets];
        m_buckets_end = m_buckets_start + num_buckets;
    }

    void fill_bucket_array_from_values() {
        for (uint32_t value_idx = 0, end_idx = m_values.size(); value_idx < end_idx; ++value_idx) {
            auto const& key = m_values[value_idx].first;
            auto [dist_and_fingerprint, bucket] = next_while_less(key);

            // we know for certain that key has not yet been inserted, so no need to check it.
            place_and_shift_up({dist_and_fingerprint, value_idx}, bucket);
        }
    }

    void increase_size() {
        --m_shifts;
        allocate_new_bucket_array();
        fill_bucket_array_from_values();
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

    auto erase(const_iterator it) -> iterator {
        auto hash = mixed_hash(it->first);
        auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
        auto* bucket = bucket_from_hash(hash);

        auto const value_idx_to_remove = static_cast<uint32_t>(it - cbegin());
        while (bucket->value_idx != value_idx_to_remove) {
            bucket = next(bucket);
        }

        do_erase(bucket);
        return begin() + (it - cbegin());
    }

    auto erase(iterator it) -> iterator {
        return erase(const_iterator{it});
    }

    auto erase(Key const& key) -> size_t {
        auto [dist_and_fingerprint, bucket] = next_while_less(key);

        while (dist_and_fingerprint == bucket->dist_and_fingerprint && !m_equals(key, m_values[bucket->value_idx].first)) {
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);
        }

        if (dist_and_fingerprint != bucket->dist_and_fingerprint) {
            // not found, nothing is deleted
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

    // TODO don't hardcode
    [[nodiscard]] auto max_load_factor() const -> float {
        return 0.8;
    }
};

} // namespace ankerl

#endif
