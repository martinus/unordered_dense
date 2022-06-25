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

// This is free and unencumbered software released into the public domain under The Unlicense (http://unlicense.org/)
// main repo: https://github.com/wangyi-fudan/wyhash
// author: 王一 Wang Yi <godspeed_china@yeah.net>
// contributors: Reini Urban, Dietrich Epp, Joshua Haberman, Tommy Ettinger, Daniel Lemire, Otmar Ertl, cocowalla, leo-yuriev,
// Diego Barrios Romero, paulie-g, dumblob, Yann Collet, ivte-ms, hyb, James Z.M. Gao, easyaspi314 (Devin), TheOneric

/* quick example:
   string s="fjsakfdsjkf";
   uint64_t hash=wyhash(s.c_str(), s.size(), 0, _wyp);
*/

#ifndef wyhash_final_version_3
#    define wyhash_final_version_3

#    ifndef WYHASH_CONDOM
// protections that produce different results:
// 1: normal valid behavior
// 2: extra protection against entropy loss (probability=2^-63), aka. "blind multiplication"
#        define WYHASH_CONDOM 1
#    endif

#    ifndef WYHASH_32BIT_MUM
// 0: normal version, slow on 32 bit systems
// 1: faster on 32 bit systems but produces different results, incompatible with wy2u0k function
#        define WYHASH_32BIT_MUM 0
#    endif

// includes
#    include <stdint.h>
#    include <string.h>
#    if defined(_MSC_VER) && defined(_M_X64)
#        include <intrin.h>
#        pragma intrinsic(_umul128)
#    endif

// likely and unlikely macros
#    if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#        define _likely_(x) __builtin_expect(x, 1)
#        define _unlikely_(x) __builtin_expect(x, 0)
#    else
#        define _likely_(x) (x)
#        define _unlikely_(x) (x)
#    endif

// 128bit multiply function
static inline uint64_t _wyrot(uint64_t x) {
    return (x >> 32) | (x << 32);
}
static inline void _wymum(uint64_t* A, uint64_t* B) {
#    if (WYHASH_32BIT_MUM)
    uint64_t hh = (*A >> 32) * (*B >> 32), hl = (*A >> 32) * (uint32_t)*B, lh = (uint32_t)*A * (*B >> 32),
             ll = (uint64_t)(uint32_t)*A * (uint32_t)*B;
#        if (WYHASH_CONDOM > 1)
    *A ^= _wyrot(hl) ^ hh;
    *B ^= _wyrot(lh) ^ ll;
#        else
    *A = _wyrot(hl) ^ hh;
    *B = _wyrot(lh) ^ ll;
#        endif
#    elif defined(__SIZEOF_INT128__)
    __uint128_t r = *A;
    r *= *B;
#        if (WYHASH_CONDOM > 1)
    *A ^= (uint64_t)r;
    *B ^= (uint64_t)(r >> 64);
#        else
    *A = (uint64_t)r;
    *B = (uint64_t)(r >> 64);
#        endif
#    elif defined(_MSC_VER) && defined(_M_X64)
#        if (WYHASH_CONDOM > 1)
    uint64_t a, b;
    a = _umul128(*A, *B, &b);
    *A ^= a;
    *B ^= b;
#        else
    *A = _umul128(*A, *B, B);
#        endif
#    else
    uint64_t ha = *A >> 32, hb = *B >> 32, la = (uint32_t)*A, lb = (uint32_t)*B, hi, lo;
    uint64_t rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb, t = rl + (rm0 << 32), c = t < rl;
    lo = t + (rm1 << 32);
    c += lo < t;
    hi = rh + (rm0 >> 32) + (rm1 >> 32) + c;
#        if (WYHASH_CONDOM > 1)
    *A ^= lo;
    *B ^= hi;
#        else
    *A = lo;
    *B = hi;
#        endif
#    endif
}

// multiply and xor mix function, aka MUM
static inline uint64_t _wymix(uint64_t A, uint64_t B) {
    _wymum(&A, &B);
    return A ^ B;
}

// endian macros
#    ifndef WYHASH_LITTLE_ENDIAN
#        if defined(_WIN32) || defined(__LITTLE_ENDIAN__) || \
            (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#            define WYHASH_LITTLE_ENDIAN 1
#        elif defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#            define WYHASH_LITTLE_ENDIAN 0
#        else
#            warning could not determine endianness! Falling back to little endian.
#            define WYHASH_LITTLE_ENDIAN 1
#        endif
#    endif

// read functions
#    if (WYHASH_LITTLE_ENDIAN)
static inline uint64_t _wyr8(const uint8_t* p) {
    uint64_t v;
    memcpy(&v, p, 8);
    return v;
}
static inline uint64_t _wyr4(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}
#    elif defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
static inline uint64_t _wyr8(const uint8_t* p) {
    uint64_t v;
    memcpy(&v, p, 8);
    return __builtin_bswap64(v);
}
static inline uint64_t _wyr4(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return __builtin_bswap32(v);
}
#    elif defined(_MSC_VER)
static inline uint64_t _wyr8(const uint8_t* p) {
    uint64_t v;
    memcpy(&v, p, 8);
    return _byteswap_uint64(v);
}
static inline uint64_t _wyr4(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return _byteswap_ulong(v);
}
#    else
static inline uint64_t _wyr8(const uint8_t* p) {
    uint64_t v;
    memcpy(&v, p, 8);
    return (((v >> 56) & 0xff) | ((v >> 40) & 0xff00) | ((v >> 24) & 0xff0000) | ((v >> 8) & 0xff000000) |
            ((v << 8) & 0xff00000000) | ((v << 24) & 0xff0000000000) | ((v << 40) & 0xff000000000000) |
            ((v << 56) & 0xff00000000000000));
}
static inline uint64_t _wyr4(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return (((v >> 24) & 0xff) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | ((v << 24) & 0xff000000));
}
#    endif
static inline uint64_t _wyr3(const uint8_t* p, size_t k) {
    return (((uint64_t)p[0]) << 16) | (((uint64_t)p[k >> 1]) << 8) | p[k - 1];
}
// wyhash main function
static inline uint64_t wyhash(const void* key, size_t len, uint64_t seed, const uint64_t* secret) {
    const uint8_t* p = (const uint8_t*)key;
    seed ^= *secret;
    uint64_t a, b;
    if (_likely_(len <= 16)) {
        if (_likely_(len >= 4)) {
            a = (_wyr4(p) << 32) | _wyr4(p + ((len >> 3) << 2));
            b = (_wyr4(p + len - 4) << 32) | _wyr4(p + len - 4 - ((len >> 3) << 2));
        } else if (_likely_(len > 0)) {
            a = _wyr3(p, len);
            b = 0;
        } else
            a = b = 0;
    } else {
        size_t i = len;
        if (_unlikely_(i > 48)) {
            uint64_t see1 = seed, see2 = seed;
            do {
                seed = _wymix(_wyr8(p) ^ secret[1], _wyr8(p + 8) ^ seed);
                see1 = _wymix(_wyr8(p + 16) ^ secret[2], _wyr8(p + 24) ^ see1);
                see2 = _wymix(_wyr8(p + 32) ^ secret[3], _wyr8(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (_likely_(i > 48));
            seed ^= see1 ^ see2;
        }
        while (_unlikely_(i > 16)) {
            seed = _wymix(_wyr8(p) ^ secret[1], _wyr8(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }
        a = _wyr8(p + i - 16);
        b = _wyr8(p + i - 8);
    }
    return _wymix(secret[1] ^ len, _wymix(a ^ secret[1], b ^ seed));
}

// the default secret parameters
static const uint64_t _wyp[4] = {0xa0761d6478bd642full, 0xe7037ed1a0b428dbull, 0x8ebc6af09c88c6e3ull, 0x589965cc75374cc3ull};

// a useful 64bit-64bit mix function to produce deterministic pseudo random numbers that can pass BigCrush and PractRand
static inline uint64_t wyhash64(uint64_t A, uint64_t B) {
    A ^= 0xa0761d6478bd642full;
    B ^= 0xe7037ed1a0b428dbull;
    _wymum(&A, &B);
    return _wymix(A ^ 0xa0761d6478bd642full, B ^ 0xe7037ed1a0b428dbull);
}

// The wyrand PRNG that pass BigCrush and PractRand
static inline uint64_t wyrand(uint64_t* seed) {
    *seed += 0xa0761d6478bd642full;
    return _wymix(*seed, *seed ^ 0xe7037ed1a0b428dbull);
}

// convert any 64 bit pseudo random numbers to uniform distribution [0,1). It can be combined with wyrand, wyhash64 or wyhash.
static inline double wy2u01(uint64_t r) {
    const double _wynorm = 1.0 / (1ull << 52);
    return (r >> 12) * _wynorm;
}

// convert any 64 bit pseudo random numbers to APPROXIMATE Gaussian distribution. It can be combined with wyrand, wyhash64 or
// wyhash.
static inline double wy2gau(uint64_t r) {
    const double _wynorm = 1.0 / (1ull << 20);
    return ((r & 0x1fffff) + ((r >> 21) & 0x1fffff) + ((r >> 42) & 0x1fffff)) * _wynorm - 3.0;
}

#    if (!WYHASH_32BIT_MUM)
// fast range integer random number generation on [0,k) credit to Daniel Lemire. May not work when WYHASH_32BIT_MUM=1. It can
// be combined with wyrand, wyhash64 or wyhash.
static inline uint64_t wy2u0k(uint64_t r, uint64_t k) {
    _wymum(&r, &k);
    return k;
}
#    endif

// make your own secret
static inline void make_secret(uint64_t seed, uint64_t* secret) {
    uint8_t c[] = {15,  23,  27,  29,  30,  39,  43,  45,  46,  51,  53,  54,  57,  58,  60,  71,  75,  77,
                   78,  83,  85,  86,  89,  90,  92,  99,  101, 102, 105, 106, 108, 113, 114, 116, 120, 135,
                   139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166, 169, 170, 172, 177, 178, 180,
                   184, 195, 197, 198, 201, 202, 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};
    for (size_t i = 0; i < 4; i++) {
        uint8_t ok;
        do {
            ok = 1;
            secret[i] = 0;
            for (size_t j = 0; j < 64; j += 8)
                secret[i] |= ((uint64_t)c[wyrand(&seed) % sizeof(c)]) << j;
            if (secret[i] % 2 == 0) {
                ok = 0;
                continue;
            }
            for (size_t j = 0; j < i; j++) {
#    if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
                if (__builtin_popcountll(secret[j] ^ secret[i]) != 32) {
                    ok = 0;
                    break;
                }
#    elif defined(_MSC_VER) && defined(_M_X64)
                if (_mm_popcnt_u64(secret[j] ^ secret[i]) != 32) {
                    ok = 0;
                    break;
                }
#    else
                // manual popcount
                uint64_t x = secret[j] ^ secret[i];
                x -= (x >> 1) & 0x5555555555555555;
                x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
                x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0f;
                x = (x * 0x0101010101010101) >> 56;
                if (x != 32) {
                    ok = 0;
                    break;
                }
#    endif
            }
        } while (!ok);
    }
}

/*  This is world's fastest hash map: 2x faster than bytell_hash_map.
    It does not store the keys, but only the hash/signature of keys.
    First we use pos=hash1(key) to approximately locate the bucket.
    Then we search signature=hash2(key) from pos linearly.
    If we find a bucket with matched signature we report the bucket
    Or if we meet a bucket whose signature=0, we report a new position to insert
    The signature collision probability is very low as we usually searched N~10 buckets.
    By combining hash1 and hash2, we acturally have 128 bit anti-collision strength.
    hash1 and hash2 can be the same function, resulting lower collision resistance but faster.
    The signature is 64 bit, but can be modified to 32 bit if necessary for save space.
    The above two can be activated by define WYHASHMAP_WEAK_SMALL_FAST
    simple examples:
    const	size_t	size=213432;
    vector<wyhashmap_t>	idx(size);	//	allocate the index of fixed size. idx MUST be zeroed.
    vector<value_class>	value(size);	//	we only care about the index, user should maintain his own value vectors.
    string  key="dhskfhdsj"	//	the object to be inserted into idx
    size_t	pos=wyhashmap(idx.data(), idx.size(), key.c_str(), key.size(), 1);	//	get the position and insert
    if(pos<size)	value[pos]++;	//	we process the vallue
    else	cerr<<"map is full\n";
    pos=wyhashmap(idx.data(), idx.size(), key.c_str(), key.size(), 0);	// just lookup by setting insert=0
    if(pos<size)	value[pos]++;	//	we process the vallue
    else	cerr<<"the key does not exist\n";
*/
/*
#ifdef	WYHASHMAP_WEAK_SMALL_FAST	// for small hashmaps whose size < 2^24 and acceptable collision
typedef	uint32_t	wyhashmap_t;
#else
typedef	uint64_t	wyhashmap_t;
#endif

static	inline	size_t	wyhashmap(wyhashmap_t	*idx,	size_t	idx_size,	const
void *key, size_t	key_size,	uint8_t	insert, uint64_t *secret){
        size_t	i=1;	uint64_t	h2;	wyhashmap_t	sig;
        do{	sig=h2=wyhash(key,key_size,i,secret);	i++;	}while(_unlikely_(!sig));
#ifdef	WYHASHMAP_WEAK_SMALL_FAST
        size_t	i0=wy2u0k(h2,idx_size);
#else
        size_t	i0=wy2u0k(wyhash(key,key_size,0,secret),idx_size);
#endif
        for(i=i0;	i<idx_size&&idx[i]&&idx[i]!=sig;	i++);
        if(_unlikely_(i==idx_size)){
                for(i=0;	i<i0&&idx[i]&&idx[i]!=sig;  i++);
                if(i==i0)	return	idx_size;
        }
        if(!idx[i]){
                if(insert)	idx[i]=sig;
                else	return	idx_size;
        }
        return	i;
}
*/
#endif

/* The Unlicense
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

namespace ankerl {

[[nodiscard]] inline auto hash_bytes(void const* ptr, size_t len) noexcept -> uint64_t {
    static constexpr uint64_t m = UINT64_C(0xc6a4a7935bd1e995);
    static constexpr uint64_t seed = UINT64_C(0xe17a1465);
    static constexpr unsigned int r = 47;

    auto const* const data64 = static_cast<uint64_t const*>(ptr);
    uint64_t h = seed ^ (len * m);

    size_t const n_blocks = len / 8;
    for (size_t i = 0; i < n_blocks; ++i) {
        uint64_t k;
        std::memcpy(&k, data64 + i, 8);

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    auto const* const data8 = reinterpret_cast<uint8_t const*>(data64 + n_blocks);
    switch (len & 7U) {
    case 7:
        h ^= static_cast<uint64_t>(data8[6]) << 48U;
    case 6:
        h ^= static_cast<uint64_t>(data8[5]) << 40U;
    case 5:
        h ^= static_cast<uint64_t>(data8[4]) << 32U;
    case 4:
        h ^= static_cast<uint64_t>(data8[3]) << 24U;
    case 3:
        h ^= static_cast<uint64_t>(data8[2]) << 16U;
    case 2:
        h ^= static_cast<uint64_t>(data8[1]) << 8U;
    case 1:
        h ^= static_cast<uint64_t>(data8[0]);
        h *= m;
    default:
        break;
    }

    h ^= h >> r;

    // not doing the final step here, because we already do a multiplication
    return static_cast<size_t>(h);
}

template <typename T, typename Enable = void>
struct hash : public std::hash<T> {
    auto operator()(T const& obj) const noexcept(noexcept(std::declval<std::hash<T>>().operator()(std::declval<T const&>())))
        -> size_t {
        auto result = std::hash<T>::operator()(obj);
    }
};

template <typename CharT>
struct hash<std::basic_string<CharT>> {
    auto operator()(std::basic_string<CharT> const& str) const noexcept -> size_t {
        // return hash_bytes(str.data(), sizeof(CharT) * str.size());
        return wyhash(str.data(), sizeof(CharT) * str.size(), 0, _wyp);
    }
};

template <typename CharT>
struct hash<std::basic_string_view<CharT>> {
    auto operator()(std::basic_string_view<CharT> const& sv) const noexcept -> size_t {
        // return hash_bytes(sv.data(), sizeof(CharT) * sv.size());
        return wyhash(sv.data(), sizeof(CharT) * sv.size(), 0, _wyp);
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
template <class Key, class T, class Hash = std::hash<Key>, class Pred = std::equal_to<Key>>
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
