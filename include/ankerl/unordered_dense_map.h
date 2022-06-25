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
#include <type_traits>
#define ANKERL_UNORDERED_DENSE_MAP_VERSION_MAJOR 0 // incompatible API changes
#define ANKERL_UNORDERED_DENSE_MAP_VERSION_MINOR 0 // add functionality in a backwards compatible manner
#define ANKERL_UNORDERED_DENSE_MAP_VERSION_PATCH 1 // backwards compatible bug fixes

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace ankerl {

/**
 * @brief
 *
 * @tparam Key
 * @tparam T
 * @tparam Hash
 * @tparam Pred
 */
template <class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>>
class unordered_dense_map {
    template <bool IsConst>
    class Iter;

public:
    using value_type = std::pair<Key, T>;
    using key_type = Key;
    using size_type = size_t;
    using const_iterator = Iter<true>;
    using iterator = Iter<false>;

private:
    using ValueContainer = std::vector<value_type>;

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

    template <bool IsConst>
    class Iter {
        using Map = typename std::conditional_t<IsConst, unordered_dense_map const, unordered_dense_map>;
        using ValuesIterator =
            typename std::conditional_t<IsConst, typename ValueContainer::const_iterator, typename ValueContainer::iterator>;
        Map* const m_map{};
        ValuesIterator m_it{};

    public:
        using difference_type = std::ptrdiff_t;
        using value_type = typename unordered_dense_map::value_type;
        using reference = typename std::conditional<IsConst, value_type const&, value_type&>::type;
        using pointer = typename std::conditional<IsConst, value_type const*, value_type*>::type;
        using iterator_category = std::forward_iterator_tag;

        Iter() = default;

        /**
         * Conversion constructor from iterator to const_iterator
         *
         * @tparam OtherIsConst
         * @tparam std::enable_if_t<IsConst && !OtherIsConst>
         * @param other
         */
        template <bool OtherIsConst, typename = typename std::enable_if_t<IsConst && !OtherIsConst>>
        Iter(Iter<OtherIsConst> const& other) // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
            : m_map(other.m_map)
            , m_it(other.m_it) {}

        Iter(Map* map, ValuesIterator const& it)
            : m_map(map)
            , m_it(it) {}

        auto operator++() -> Iter& {
            ++m_it;
            return *this;
        }

        auto operator++(int) -> Iter {
            return {m_map, m_it++};
        }

        auto operator*() const -> reference {
            return *m_it;
        }

        auto operator->() const -> pointer {
            return &*m_it;
        }

        template <bool O>
        auto operator==(Iter<O> const& other) const -> bool {
            return m_it == other.m_it;
        }

        template <bool O>
        auto operator!=(Iter<O> const& other) const -> bool {
            return m_it != other.m_it;
        }
    };

    [[nodiscard]] auto next(Bucket const* bucket) const -> Bucket const* {
        if (++bucket == m_buckets_end) {
            return m_buckets_start;
        }
        return bucket;
    }

    [[nodiscard]] auto next(Bucket* bucket) -> Bucket* {
        if (++bucket == m_buckets_end) {
            return m_buckets_start;
        }
        return bucket;
    }

    [[nodiscard]] constexpr auto mixed_hash(Key const& key) const -> uint64_t {
        return static_cast<uint64_t>(m_hash(key)) * UINT64_C(0x9E3779B97F4A7C15);
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

public:
    unordered_dense_map() = default;

    ~unordered_dense_map() {
        delete[] m_buckets_start;
    }

    template <typename K>
    auto find(K const& key) const -> const_iterator {
        auto mh = mixed_hash(key);
        auto dist_and_fingerprint = dist_and_fingerprint_from_hash(mh);
        auto const* bucket = bucket_from_hash(mh);

        do {
            if (dist_and_fingerprint == bucket->dist_and_fingerprint && m_equals(key, m_values[bucket->value_idx].first)) {
                return {this, m_values.begin() + bucket->value_idx};
            }
            dist_and_fingerprint += BUCKET_DIST_INC;
            bucket = next(bucket);
        } while (dist_and_fingerprint <= bucket->dist_and_fingerprint);
        return {this, m_values.end()};
    }

    auto cend() const -> const_iterator {
        return {}
    }
    auto end() const -> const_iterator {
        return cend();
    }

    auto operator[](Key const& key) -> T& {
        return try_emplace(key).first->second;
    }

    auto insert(value_type const& value) -> std::pair<iterator, bool> {
        return try_emplace(value.first, value.second);
    }

    void place_and_shift_up(Bucket bucket, Bucket* place) {
        while (0 != place->dist_and_fingerprint) {
            bucket = std::exchange(*place, bucket);
            bucket.dist_and_fingerprint += BUCKET_DIST_INC;
            place = next(place);
        }
        *place = bucket;
    }

    template <typename K, typename... Args>
    auto try_emplace(K&& key, Args&&... args) -> std::pair<iterator, bool> {
        if (is_full()) {
            increase_size();
        }

        auto [dist_and_fingerprint, bucket] = next_while_less(key);

        while (dist_and_fingerprint == bucket->dist_and_fingerprint) {
            if (m_equals(key, m_values[bucket->value_idx].first)) {
                return {iterator{this, m_values.begin() + bucket->value_idx}, false};
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

        return {iterator{this, m_values.begin() + value_idx}, true};
    }

    /**
     * True when no element can be added any more without increasing the size
     */
    [[nodiscard]] auto is_full() const -> bool {
        return size() == m_max_bucket_capacity;
    }

    void increase_size() {
        // just discard the whole bucket array and create a new one
        delete[] m_buckets_start;

        // increase size 2x
        --m_shifts;
        uint64_t num_buckets = UINT64_C(1) << (64U - m_shifts);
        m_max_bucket_capacity = static_cast<uint64_t>(num_buckets * max_load_factor());
        m_buckets_start = new Bucket[num_buckets];
        m_buckets_end = m_buckets_start + num_buckets;

        for (uint32_t value_idx = 0, end_idx = m_values.size(); value_idx < end_idx; ++value_idx) {
            auto const& key = m_values[value_idx].first;
            auto [dist_and_fingerprint, bucket] = next_while_less(key);

            // we know for certain that key has not yet been inserted, so no need to check it.
            place_and_shift_up({dist_and_fingerprint, value_idx}, bucket);
        }
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
        return 1;
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
