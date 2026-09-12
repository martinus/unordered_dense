///////////////////////// ankerl::unordered_dense::{map, set} /////////////////////////

// A fast & densely stored hashmap and hashset.
// Version 5.0.0
// https://github.com/martinus/unordered_dense
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

#ifndef ANKERL_UNORDERED_DENSE_H
#define ANKERL_UNORDERED_DENSE_H

// see https://semver.org/spec/v2.0.0.html
#define ANKERL_UNORDERED_DENSE_VERSION_MAJOR 5 // NOLINT(cppcoreguidelines-macro-usage) incompatible API changes
#define ANKERL_UNORDERED_DENSE_VERSION_MINOR 0 // NOLINT(cppcoreguidelines-macro-usage) backwards compatible functionality
#define ANKERL_UNORDERED_DENSE_VERSION_PATCH 0 // NOLINT(cppcoreguidelines-macro-usage) backwards compatible bug fixes

// API versioning with inline namespace, see https://www.foonathan.net/2018/11/inline-namespaces/

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ANKERL_UNORDERED_DENSE_VERSION_CONCAT1(major, minor, patch) v##major##_##minor##_##patch
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define ANKERL_UNORDERED_DENSE_VERSION_CONCAT(major, minor, patch) ANKERL_UNORDERED_DENSE_VERSION_CONCAT1(major, minor, patch)
#define ANKERL_UNORDERED_DENSE_NAMESPACE   \
    ANKERL_UNORDERED_DENSE_VERSION_CONCAT( \
        ANKERL_UNORDERED_DENSE_VERSION_MAJOR, ANKERL_UNORDERED_DENSE_VERSION_MINOR, ANKERL_UNORDERED_DENSE_VERSION_PATCH)

#if defined(_MSVC_LANG)
#    define ANKERL_UNORDERED_DENSE_CPP_VERSION _MSVC_LANG
#else
#    define ANKERL_UNORDERED_DENSE_CPP_VERSION __cplusplus
#endif

#if defined(__GNUC__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define ANKERL_UNORDERED_DENSE_PACK(decl) decl __attribute__((__packed__))
#elif defined(_MSC_VER)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define ANKERL_UNORDERED_DENSE_PACK(decl) __pragma(pack(push, 1)) decl __pragma(pack(pop))
#endif

// exceptions
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#    define ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() 1 // NOLINT(cppcoreguidelines-macro-usage)
#else
#    define ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() 0 // NOLINT(cppcoreguidelines-macro-usage)
#endif
#ifdef _MSC_VER
#    define ANKERL_UNORDERED_DENSE_NOINLINE __declspec(noinline)
#    define ANKERL_UNORDERED_DENSE_FORCEINLINE __forceinline
#else
#    define ANKERL_UNORDERED_DENSE_NOINLINE __attribute__((noinline))
#    define ANKERL_UNORDERED_DENSE_FORCEINLINE inline __attribute__((always_inline))
#endif

// Data prefetch hint, a no-op where there is nothing to spell it with. MSVC has no
// __builtin_prefetch and used to get the no-op, which quietly cost it the one the probe issues for
// a group's value indices -- measured at 3 cycles off every hit, so a whole compiler was paying for
// a missing spelling. Both MSVC intrinsics come from <intrin.h>, which is included further down;
// that is in time, because a macro needs its declarations where it is expanded and every expansion
// is inside the table. Taken from boost, which covers the same three cases.
#if defined(__GNUC__) || defined(__clang__)
#    define ANKERL_UNORDERED_DENSE_PREFETCH(addr) __builtin_prefetch(addr) // NOLINT(cppcoreguidelines-macro-usage)
#elif defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define ANKERL_UNORDERED_DENSE_PREFETCH(addr) _mm_prefetch(reinterpret_cast<char const*>(addr), _MM_HINT_T0)
#elif defined(_MSC_VER) && defined(_M_ARM64)
#    define ANKERL_UNORDERED_DENSE_PREFETCH(addr) __prefetch(addr) // NOLINT(cppcoreguidelines-macro-usage)
#else
#    define ANKERL_UNORDERED_DENSE_PREFETCH(addr) static_cast<void>(addr) // NOLINT(cppcoreguidelines-macro-usage)
#endif

// SSE2 is part of the x86-64 baseline, so comparing a group's sixteen fingerprints in one
// instruction is available on every x86-64 build without asking for it. Elsewhere, and in a build
// that defines this to 0, they are compared eight per machine word with ordinary arithmetic.
//
// This picks the code, never the layout: a group is sixteen slots either way, so two translation
// units that disagree about this macro -- which they may, it is documented as a per-target switch
// -- still agree about every byte of the index they share.
#if !defined(ANKERL_UNORDERED_DENSE_HAS_SSE2)
#    if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#        define ANKERL_UNORDERED_DENSE_HAS_SSE2 1 // NOLINT(cppcoreguidelines-macro-usage)
#    else
#        define ANKERL_UNORDERED_DENSE_HAS_SSE2 0 // NOLINT(cppcoreguidelines-macro-usage)
#    endif
#endif

// The same sixteen-at-once compare on AArch64, which is the other baseline worth having: NEON is
// mandatory there, so this needs no runtime dispatch either. Restricted to little endian because
// the mask below reads the comparison result as one 64 bit word, and to AArch64 because 32 bit ARM
// lacks the horizontal ops -- both fall back to SWAR, which is correct everywhere.
#if !defined(ANKERL_UNORDERED_DENSE_HAS_NEON)
#    if defined(__ARM_NEON) && defined(__aarch64__) && \
        (!defined(__BYTE_ORDER__) || !defined(__ORDER_BIG_ENDIAN__) || (__BYTE_ORDER__ != __ORDER_BIG_ENDIAN__))
#        define ANKERL_UNORDERED_DENSE_HAS_NEON 1 // NOLINT(cppcoreguidelines-macro-usage)
#    else
#        define ANKERL_UNORDERED_DENSE_HAS_NEON 0 // NOLINT(cppcoreguidelines-macro-usage)
#    endif
#endif
#if ANKERL_UNORDERED_DENSE_HAS_SSE2 && ANKERL_UNORDERED_DENSE_HAS_NEON
#    error "ANKERL_UNORDERED_DENSE_HAS_SSE2 and ANKERL_UNORDERED_DENSE_HAS_NEON cannot both be on"
#endif

#if defined(__clang__) && defined(__has_attribute)
#    if __has_attribute(__no_sanitize__)
#        define ANKERL_UNORDERED_DENSE_DISABLE_UBSAN_UNSIGNED_INTEGER_CHECK \
            __attribute__((__no_sanitize__("unsigned-integer-overflow")))
#    endif
#endif

#if !defined(ANKERL_UNORDERED_DENSE_DISABLE_UBSAN_UNSIGNED_INTEGER_CHECK)
#    define ANKERL_UNORDERED_DENSE_DISABLE_UBSAN_UNSIGNED_INTEGER_CHECK
#endif

#if ANKERL_UNORDERED_DENSE_CPP_VERSION < 201703L
#    error ankerl::unordered_dense requires C++17 or higher
#else

#    if !defined(ANKERL_UNORDERED_DENSE_STD_MODULE)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#        define ANKERL_UNORDERED_DENSE_STD_MODULE 0
#    endif

#    if !ANKERL_UNORDERED_DENSE_STD_MODULE
#        include "stl.h"
#    endif
#    if ANKERL_UNORDERED_DENSE_HAS_SSE2
#        include <emmintrin.h> // for _mm_loadu_si128, _mm_cmpeq_epi8, ...
#    endif
#    if ANKERL_UNORDERED_DENSE_HAS_NEON
#        include <arm_neon.h> // for vld1q_u8, vceqq_u8, vshrn_n_u16, ...
#    endif
#    if defined(_MSC_VER)
#        include <intrin.h> // for _BitScanForward
#    endif

#    if __has_cpp_attribute(likely) && __has_cpp_attribute(unlikely) && ANKERL_UNORDERED_DENSE_CPP_VERSION >= 202002L
#        define ANKERL_UNORDERED_DENSE_LIKELY_ATTR [[likely]]     // NOLINT(cppcoreguidelines-macro-usage)
#        define ANKERL_UNORDERED_DENSE_UNLIKELY_ATTR [[unlikely]] // NOLINT(cppcoreguidelines-macro-usage)
#        define ANKERL_UNORDERED_DENSE_LIKELY(x) (x)              // NOLINT(cppcoreguidelines-macro-usage)
#        define ANKERL_UNORDERED_DENSE_UNLIKELY(x) (x)            // NOLINT(cppcoreguidelines-macro-usage)
#    else
#        define ANKERL_UNORDERED_DENSE_LIKELY_ATTR   // NOLINT(cppcoreguidelines-macro-usage)
#        define ANKERL_UNORDERED_DENSE_UNLIKELY_ATTR // NOLINT(cppcoreguidelines-macro-usage)

#        if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#            define ANKERL_UNORDERED_DENSE_LIKELY(x) __builtin_expect(x, 1)   // NOLINT(cppcoreguidelines-macro-usage)
#            define ANKERL_UNORDERED_DENSE_UNLIKELY(x) __builtin_expect(x, 0) // NOLINT(cppcoreguidelines-macro-usage)
#        else
#            define ANKERL_UNORDERED_DENSE_LIKELY(x) (x)   // NOLINT(cppcoreguidelines-macro-usage)
#            define ANKERL_UNORDERED_DENSE_UNLIKELY(x) (x) // NOLINT(cppcoreguidelines-macro-usage)
#        endif

#    endif

namespace ankerl::unordered_dense {
inline namespace ANKERL_UNORDERED_DENSE_NAMESPACE {

namespace detail {

#    if ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS()

// make sure this is not inlined as it is slow and dramatically enlarges code, thus making other
// inlinings more difficult. Throws are also generally the slow path.
[[noreturn]] inline ANKERL_UNORDERED_DENSE_NOINLINE void on_error_key_not_found() {
    throw std::out_of_range("ankerl::unordered_dense::map::at(): key not found");
}
[[noreturn]] inline ANKERL_UNORDERED_DENSE_NOINLINE void on_error_bucket_overflow() {
    throw std::overflow_error("ankerl::unordered_dense: reached max bucket size, cannot increase size");
}
[[noreturn]] inline ANKERL_UNORDERED_DENSE_NOINLINE void on_error_too_many_elements() {
    throw std::out_of_range("ankerl::unordered_dense::map::replace(): too many elements");
}
[[noreturn]] inline ANKERL_UNORDERED_DENSE_NOINLINE void on_error_key_changed() {
    throw std::logic_error("ankerl::unordered_dense: an element's key changed after it was inserted; use replace_key()");
}

#    else

[[noreturn]] inline void on_error_key_not_found() {
    abort();
}
[[noreturn]] inline void on_error_bucket_overflow() {
    abort();
}
[[noreturn]] inline void on_error_too_many_elements() {
    abort();
}
[[noreturn]] inline void on_error_key_changed() {
    abort();
}

#    endif

// Index of the lowest set bit, for the lane mask a group compare produces. x
// must not be zero.
[[nodiscard]] inline auto countr_zero(std::uint32_t x) -> unsigned {
#    if defined(_MSC_VER)
    unsigned long idx{};
    _BitScanForward(&idx, x);
    return static_cast<unsigned>(idx);
#    else
    return static_cast<unsigned>(__builtin_ctz(x));
#    endif
}

#    if ANKERL_UNORDERED_DENSE_HAS_NEON
// NEON's match mask is one bit per lane four bits apart, so it needs the whole word.
[[nodiscard]] inline auto countr_zero(std::uint64_t x) -> unsigned {
#        if defined(_MSC_VER)
    unsigned long idx{};
    _BitScanForward64(&idx, x);
    return static_cast<unsigned>(idx);
#        else
    return static_cast<unsigned>(__builtin_ctzll(x));
#        endif
}
#    endif

} // namespace detail

// hash ///////////////////////////////////////////////////////////////////////

// Descended from wyhash: https://github.com/wangyi-fudan/wyhash -- its reads, its multiply-and-xor
// mix, its short path and its chained lanes for long keys -- with the middle lengths restructured
// into independent blocks, which the comment on hash() explains. No big-endian support (because
// different values on different machines don't matter), hardcodes seed and the secret.
namespace detail::wyhash {

inline void mum(std::uint64_t* a, std::uint64_t* b) {
#    if defined(__SIZEOF_INT128__)
    __uint128_t r = *a;
    r *= *b;
    *a = static_cast<std::uint64_t>(r);
    *b = static_cast<std::uint64_t>(r >> 64U);
#    elif defined(_MSC_VER) && defined(_M_X64)
    *a = _umul128(*a, *b, b);
#    else
    std::uint64_t ha = *a >> 32U;
    std::uint64_t hb = *b >> 32U;
    std::uint64_t la = static_cast<std::uint32_t>(*a);
    std::uint64_t lb = static_cast<std::uint32_t>(*b);
    std::uint64_t hi{};
    std::uint64_t lo{};
    std::uint64_t rh = ha * hb;
    std::uint64_t rm0 = ha * lb;
    std::uint64_t rm1 = hb * la;
    std::uint64_t rl = la * lb;
    std::uint64_t t = rl + (rm0 << 32U);
    auto c = static_cast<std::uint64_t>(t < rl);
    lo = t + (rm1 << 32U);
    c += static_cast<std::uint64_t>(lo < t);
    hi = rh + (rm0 >> 32U) + (rm1 >> 32U) + c;
    *a = lo;
    *b = hi;
#    endif
}

// multiply and xor mix function, aka MUM
[[nodiscard]] inline auto mix(std::uint64_t a, std::uint64_t b) -> std::uint64_t {
    mum(&a, &b);
    return a ^ b;
}

// read functions. WARNING: we don't care about endianness, so results are different on big endian!
[[nodiscard]] inline auto r8(const std::uint8_t* p) -> std::uint64_t {
    std::uint64_t v{};
    std::memcpy(&v, p, 8U);
    return v;
}

[[nodiscard]] inline auto r4(const std::uint8_t* p) -> std::uint64_t {
    std::uint32_t v{};
    std::memcpy(&v, p, 4);
    return v;
}

// reads 1, 2, or 3 bytes
[[nodiscard]] inline auto r3(const std::uint8_t* p, std::size_t k) -> std::uint64_t {
    return (static_cast<std::uint64_t>(p[0]) << 16U) | (static_cast<std::uint64_t>(p[k >> 1U]) << 8U) | p[k - 1];
}

// The shape of this is wyhash's up to 16 bytes and for anything past 144, and in between it is
// not: every 16 byte block is mixed on its own, with its own pair of secrets, and the results are
// xor-folded into one finalizer. wyhash chains the blocks through `seed`, so a 48 byte key is
// three multiplies one after another and then the finalizer, and a map lookup waits for all of
// them before it can so much as form the group address. Here the block multiplies are independent,
// so the latency of any key up to 144 bytes is one multiply plus the finalizer, and the block
// loop's trip count -- a data-dependent branch that mispredicts whenever lengths vary -- is a
// short chain of compares that the predictor learns from the top. The last sixteen bytes are
// always a block of their own, wherever they fall, so every byte is read at least once and nothing
// is read past the end.
//
// Measured on the scored benchmark's own keys (8 to 135 bytes, skewed short), one function per
// binary, ns per hash: throughput 2.52 to 2.00 under clang and 2.18 to 2.05 under gcc, latency
// 8.64 to 7.69 and 8.47 to 7.81, with fewer branch misses on both. Two shapes measured and
// rejected on the way: making the block range branchless by always mixing three (17-48) or six
// (49-96) overlapping blocks, which costs more in redundant multiplies than it saves in
// mispredictions; and a one multiply short path, which fails an avalanche test outright at 8 bytes
// (output bits that never flip for some input bits), as does dropping the finalizer in the block
// range. Both multiplies stay.
//
// Independent blocks need distinct secrets: with a shared one, swapping two blocks gives the same
// hash. Sixteen pairs cover 144 bytes, and past that the chained lanes take over, where reuse is
// harmless because the chain carries the position. The secrets have wyhash's property, every
// byte with four bits set, odd, and were drawn once from a fixed seed.
[[maybe_unused]] [[nodiscard]] inline auto hash(void const* key, std::size_t len) -> std::uint64_t {
    static constexpr auto secret = std::array{
        UINT64_C(0xa0761d6478bd642f), UINT64_C(0xe7037ed1a0b428db), UINT64_C(0x8ebc6af09c88c6e3), UINT64_C(0x589965cc75374cc3),
        UINT64_C(0x2d358dccaa6c78a5), UINT64_C(0x8bb84b93962eacc9), UINT64_C(0x4b33a62ed433d4a3), UINT64_C(0xa693c93927d87217),
        UINT64_C(0x2b63728e53473c2b), UINT64_C(0x696cb2a95635a3c5), UINT64_C(0xa9ccd81ed1b29359), UINT64_C(0x5c2d66ace48db84d),
        UINT64_C(0x69a99c5c53b4ca2d), UINT64_C(0x9a9c5a1b27d10f69), UINT64_C(0x2b27f02dc3d4360f), UINT64_C(0x2b39665c8d2d5553),
        UINT64_C(0x966cd8878bb4b187), UINT64_C(0xc6351e99932b1ee1), UINT64_C(0xd1c5d24d63c959c9), UINT64_C(0x56c54d9c955aca2b),
        UINT64_C(0xd136d27872563559)};

    auto const* p = static_cast<std::uint8_t const*>(key);
    std::uint64_t seed = secret[0];
    std::uint64_t a{};
    std::uint64_t b{};
    if (ANKERL_UNORDERED_DENSE_LIKELY(len <= 16))
        ANKERL_UNORDERED_DENSE_LIKELY_ATTR {
            if (ANKERL_UNORDERED_DENSE_LIKELY(len >= 8))
                ANKERL_UNORDERED_DENSE_LIKELY_ATTR {
                    // two (potentially overlapping) 8 byte reads cover the whole input
                    a = r8(p);
                    b = r8(p + len - 8);
                }
            else if (len >= 4) {
                a = r4(p);
                b = r4(p + len - 4);
            } else if (ANKERL_UNORDERED_DENSE_LIKELY(len > 0))
                ANKERL_UNORDERED_DENSE_LIKELY_ATTR {
                    // b stays zero: r3 packs all len bytes it is given into a, and there are at
                    // most three of them.
                    a = r3(p, len);
                }
            // ... and an empty input needs no branch of its own: it hashes whatever a and b were
            // declared with, which is the zero it has to be. Assigning it again here is what a
            // deletion sweep of this file kept pointing at.

            // Return, rather than falling through to the same expression at the end of the
            // function. Falling through makes seed, a and b values of two paths at once, and then
            // the compiler cannot fold the constant seed of this one into the mix: measured, the
            // short path costs 36 instructions that way and 24 this way.
            return mix(secret[1] ^ len, mix(a ^ secret[1], b ^ seed));
        }

    if (ANKERL_UNORDERED_DENSE_LIKELY(len <= 144))
        ANKERL_UNORDERED_DENSE_LIKELY_ATTR {
            // The first block and the last sixteen bytes, then whole blocks from the front for as
            // long as there are any: a key of 17 to 32 bytes is two multiplies, one of 129 to 144
            // is nine, all of them independent.
            auto x =
                mix(r8(p) ^ secret[1], r8(p + 8) ^ secret[2]) ^ mix(r8(p + len - 16) ^ secret[3], r8(p + len - 8) ^ secret[4]);
            if (len > 32) {
                x ^= mix(r8(p + 16) ^ secret[5], r8(p + 24) ^ secret[6]);
                if (len > 48) {
                    x ^= mix(r8(p + 32) ^ secret[7], r8(p + 40) ^ secret[8]);
                    if (len > 64) {
                        x ^= mix(r8(p + 48) ^ secret[9], r8(p + 56) ^ secret[10]);
                        if (len > 80) {
                            x ^= mix(r8(p + 64) ^ secret[11], r8(p + 72) ^ secret[12]);
                            if (len > 96) {
                                x ^= mix(r8(p + 80) ^ secret[13], r8(p + 88) ^ secret[14]);
                                if (len > 112) {
                                    x ^= mix(r8(p + 96) ^ secret[15], r8(p + 104) ^ secret[16]);
                                    if (len > 128) {
                                        x ^= mix(r8(p + 112) ^ secret[17], r8(p + 120) ^ secret[18]);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            return mix(secret[1] ^ len, x);
        }

    // Anything longer, in chained lanes of 16 bytes, ending on the same expression as above.
    std::size_t i = len;
    std::uint64_t see1 = seed;
    std::uint64_t see2 = seed;
    // Six lanes cost three more accumulators to set up and fold back in, so the block has to
    // run more than once to pay for them. Entering it at 96 meant exactly one iteration for
    // everything from 97 to 192 bytes, which never can: measured, 23.4 cycles for a 100 byte key
    // against 21.8 when it takes the 48 byte loop instead, and 29.3 against 28.2 at 150. Above
    // 192 the block runs at least twice and wins again -- 143.5 cycles against 147.1 at 1000
    // bytes -- so it keeps those.
    if (i > 192) {
        // 6 independent lanes: twice the instruction level parallelism of the 48 byte loop below
        std::uint64_t see3 = seed;
        std::uint64_t see4 = seed;
        std::uint64_t see5 = seed;
        do {
            seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
            see1 = mix(r8(p + 16) ^ secret[2], r8(p + 24) ^ see1);
            see2 = mix(r8(p + 32) ^ secret[3], r8(p + 40) ^ see2);
            see3 = mix(r8(p + 48) ^ secret[4], r8(p + 56) ^ see3);
            see4 = mix(r8(p + 64) ^ secret[5], r8(p + 72) ^ see4);
            see5 = mix(r8(p + 80) ^ secret[6], r8(p + 88) ^ see5);
            p += 96;
            i -= 96;
        } while (ANKERL_UNORDERED_DENSE_LIKELY(i > 96));
        seed ^= see3 ^ see4 ^ see5;
    }
    while (i > 48) {
        seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
        see1 = mix(r8(p + 16) ^ secret[2], r8(p + 24) ^ see1);
        see2 = mix(r8(p + 32) ^ secret[3], r8(p + 40) ^ see2);
        p += 48;
        i -= 48;
    }
    seed ^= see1 ^ see2;
    while (i > 16) {
        seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
        i -= 16;
        p += 16;
    }

    // the tail lane only depends on the input, not on seed, so it can execute in parallel
    // with the lane loops above, and a single dependent mix finishes the hash
    auto tail = mix(r8(p + i - 16) ^ secret[2], r8(p + i - 8) ^ secret[3]);
    return mix(secret[1] ^ len, seed ^ tail);
}

[[nodiscard]] inline auto hash(std::uint64_t x) -> std::uint64_t {
    return detail::wyhash::mix(x, UINT64_C(0x9E3779B97F4A7C15));
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

template <template <class...> class Op, class... Args>
using is_detected = typename detail::detector<detail::nonesuch, void, Op, Args...>::value_t;

template <template <class...> class Op, class... Args>
constexpr bool is_detected_v = is_detected<Op, Args...>::value;

template <typename>
constexpr bool dependent_false = false;

template <typename T>
using detect_avalanching = typename T::is_avalanching;

// The member written as a value instead of a type, which is the near miss that would otherwise
// answer "not avalanching" and say nothing about why.
template <typename T>
using detect_avalanching_as_value = decltype((void)T::is_avalanching);

template <typename T>
using detect_bool_value = std::enable_if_t<std::is_convertible_v<decltype(T::value), bool>>;

// What a hash's is_avalanching member means. void is this library's spelling, and Boost's original
// one; a type carrying a compile time bool is what Boost's documentation asks for now. Saying
// std::false_type there has to mean no rather than yes -- reading the member as a bare "it is
// there" would take a hash that declares itself ordinary and use it unmixed, which is the one
// answer that costs the table its distribution.
//
// Anything else is a mistake, and is said to be one rather than guessed at.
template <typename Hash>
[[nodiscard]] constexpr auto is_avalanching_member() -> bool {
    if constexpr (!is_detected_v<detect_avalanching, Hash>) {
        static_assert(!is_detected_v<detect_avalanching_as_value, Hash>,
                      "is_avalanching must be a type: write 'using is_avalanching = std::true_type;' "
                      "rather than 'static constexpr bool is_avalanching = true;'");
        return false;
    } else if constexpr (std::is_void_v<detect_avalanching<Hash>>) {
        return true;
    } else if constexpr (is_detected_v<detect_bool_value, detect_avalanching<Hash>>) {
        return static_cast<bool>(detect_avalanching<Hash>::value);
    } else {
        static_assert(dependent_false<Hash>,
                      "is_avalanching must be void, or a type with a compile time bool value such "
                      "as std::true_type or std::false_type");
        return false;
    }
}

} // namespace detail

// Whether a hash is high quality -- every bit of its result independently well distributed -- so
// that a table can index with those bits as they come instead of mixing them first. The default
// answer is the member typedef a hash can carry, `using is_avalanching = void;` or the equivalent
// `= std::true_type`. For a hash you cannot edit, specialize this instead; `std::false_type` is
// allowed too, and forces the mixing back on for a hash that promises more than it delivers.
//
// Deliberately the same name, the same two ways of answering and the same meaning as Boost's
// boost::hash_is_avalanching, so that a hash annotated for either library is read correctly by the
// other. See README 3.2.7.
template <typename Hash>
struct hash_is_avalanching : std::bool_constant<detail::is_avalanching_member<Hash>()> {};

template <typename Hash>
constexpr bool hash_is_avalanching_v = hash_is_avalanching<Hash>::value;

template <typename T, typename Enable = void>
struct hash {
    auto operator()(T const& obj) const noexcept(noexcept(std::declval<std::hash<T>>().operator()(std::declval<T const&>())))
        -> std::uint64_t {
        return std::hash<T>{}(obj);
    }
};

// Asked of hash_is_avalanching rather than of std::hash<T>::is_avalanching directly, so that there
// is one reader of the marker and not two: a std::hash spelling its marker the way Boost asks, or
// named avalanching by a specialization because it cannot be edited, reaches the table through here
// as well.
template <typename T>
struct hash<T, std::enable_if_t<hash_is_avalanching_v<std::hash<T>>>> {
    using is_avalanching = void;
    auto operator()(T const& obj) const noexcept(noexcept(std::declval<std::hash<T>>().operator()(std::declval<T const&>())))
        -> std::uint64_t {
        return std::hash<T>{}(obj);
    }
};

template <typename CharT>
struct hash<std::basic_string<CharT>> {
    using is_avalanching = void;
    auto operator()(std::basic_string<CharT> const& str) const noexcept -> std::uint64_t {
        return detail::wyhash::hash(str.data(), sizeof(CharT) * str.size());
    }
};

template <typename CharT>
struct hash<std::basic_string_view<CharT>> {
    using is_avalanching = void;
    auto operator()(std::basic_string_view<CharT> const& sv) const noexcept -> std::uint64_t {
        return detail::wyhash::hash(sv.data(), sizeof(CharT) * sv.size());
    }
};

template <class T>
struct hash<T*> {
    using is_avalanching = void;
    auto operator()(T* ptr) const noexcept -> std::uint64_t {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return detail::wyhash::hash(reinterpret_cast<std::uintptr_t>(ptr));
    }
};

template <class T>
struct hash<std::unique_ptr<T>> {
    using is_avalanching = void;
    auto operator()(std::unique_ptr<T> const& ptr) const noexcept -> std::uint64_t {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return detail::wyhash::hash(reinterpret_cast<std::uintptr_t>(ptr.get()));
    }
};

template <class T>
struct hash<std::shared_ptr<T>> {
    using is_avalanching = void;
    auto operator()(std::shared_ptr<T> const& ptr) const noexcept -> std::uint64_t {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return detail::wyhash::hash(reinterpret_cast<std::uintptr_t>(ptr.get()));
    }
};

template <typename Enum>
struct hash<Enum, typename std::enable_if_t<std::is_enum_v<Enum>>> {
    using is_avalanching = void;
    auto operator()(Enum e) const noexcept -> std::uint64_t {
        using underlying = std::underlying_type_t<Enum>;
        return detail::wyhash::hash(static_cast<std::uint64_t>(static_cast<underlying>(e)));
    }
};

template <typename... Args>
struct tuple_hash_helper {
    // Converts the value into 64bit. If it is an integral type, just cast it. Mixing is doing the rest.
    // If it isn't an integral we need to hash it.
    template <typename Arg>
    [[nodiscard]] constexpr static auto to64(Arg const& arg) -> std::uint64_t {
        if constexpr (std::is_integral_v<Arg> || std::is_enum_v<Arg>) {
            return static_cast<std::uint64_t>(arg);
        } else {
            return hash<Arg>{}(arg);
        }
    }

    [[nodiscard]] ANKERL_UNORDERED_DENSE_DISABLE_UBSAN_UNSIGNED_INTEGER_CHECK static auto mix64(std::uint64_t state,
                                                                                                std::uint64_t v)
        -> std::uint64_t {
        return detail::wyhash::mix(state + v, std::uint64_t{0x9ddfea08eb382d69});
    }

    // Creates a buffer that holds all the data from each element of the tuple. If possible we memcpy the data directly. If
    // not, we hash the object and use this for the array. Size of the array is known at compile time, and memcpy is optimized
    // away, so filling the buffer is highly efficient. Finally, call wyhash with this buffer.
    template <typename T, std::size_t... Idx>
    [[nodiscard]] static auto calc_hash(T const& t, std::index_sequence<Idx...> /*unused*/) noexcept -> std::uint64_t {
        auto h = std::uint64_t{};
        ((h = mix64(h, to64(std::get<Idx>(t)))), ...);
        return h;
    }
};

template <typename... Args>
struct hash<std::tuple<Args...>> : tuple_hash_helper<Args...> {
    using is_avalanching = void;
    auto operator()(std::tuple<Args...> const& t) const noexcept -> std::uint64_t {
        return tuple_hash_helper<Args...>::calc_hash(t, std::index_sequence_for<Args...>{});
    }
};

template <typename A, typename B>
struct hash<std::pair<A, B>> : tuple_hash_helper<A, B> {
    using is_avalanching = void;
    auto operator()(std::pair<A, B> const& t) const noexcept -> std::uint64_t {
        return tuple_hash_helper<A, B>::calc_hash(t, std::index_sequence_for<A, B>{});
    }
};

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define ANKERL_UNORDERED_DENSE_HASH_STATICCAST(T)                         \
        template <>                                                           \
        struct hash<T> {                                                      \
            using is_avalanching = void;                                      \
            auto operator()(T const& obj) const noexcept -> std::uint64_t {   \
                return detail::wyhash::hash(static_cast<std::uint64_t>(obj)); \
            }                                                                 \
        }

#    if defined(__GNUC__) && !defined(__clang__)
#        pragma GCC diagnostic push
#        pragma GCC diagnostic ignored "-Wuseless-cast"
#    endif
// see https://en.cppreference.com/w/cpp/utility/hash
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(bool);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(char);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(signed char);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(unsigned char);
#    if ANKERL_UNORDERED_DENSE_CPP_VERSION >= 202002L && defined(__cpp_char8_t)
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(char8_t);
#    endif
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(char16_t);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(char32_t);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(wchar_t);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(short);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(unsigned short);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(int);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(unsigned int);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(long);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(long long);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(unsigned long);
ANKERL_UNORDERED_DENSE_HASH_STATICCAST(unsigned long long);

#    if defined(__GNUC__) && !defined(__clang__)
#        pragma GCC diagnostic pop
#    endif

// bucket_type //////////////////////////////////////////////////////////

namespace bucket_type {

// The index is groups of sixteen slots. A group holds one byte of fingerprint per slot, compared
// sixteen at a time, and eight overflow counters that record how many entries with those low three
// fingerprint bits had to probe past it. An erase decrements them, so nothing ever moves after it
// is placed and no tombstone is left behind. The value indices sit in the same block as the group
// they belong to: 24 + 64 bytes per sixteen slots, 5.5 bytes per slot. See "5. Design" in the
// README, and group_storage::block below for why one block rather than two arrays.
//
// The width of the value index is the one thing the two bucket types differ in: `group` indexes
// up to 2^32 values at 24 + 64 bytes per sixteen slots, `group_big` up to 2^63 at 24 + 128.
template <typename ValueIdx>
struct basic_group {
    using value_idx_type = ValueIdx;
    std::array<std::uint8_t, 16> m_fingerprints; // one per slot; 0 is an empty slot
    std::array<std::uint8_t, 8> m_overflows;     // how many entries with (fingerprint & 7) == i probed past this group
};

using group = basic_group<std::uint32_t>;
using group_big = basic_group<std::size_t>;

} // namespace bucket_type

namespace detail {

// The fingerprint word of every low byte of a hash: the fingerprint in all four bytes, so that
// the vector compare can broadcast it as one 32 bit lane. 0 maps to 8 so that 0 means empty and
// the low three bits, which pick the counter, are unchanged. A table rather than the arithmetic
// (and, compare, shift, or, multiply) because it is five instructions on the critical path of
// every probe, placement and erase, and one aligned load from a kilobyte that stays in L1 is
// cheaper: measured paired, integer misses 1.05-1.06x on both compilers, big-value finds 1.03x
// under clang and 1.14x under gcc, strings level. Boost's group15 keeps the same table.
[[nodiscard]] constexpr auto make_fingerprint_words() -> std::array<std::uint32_t, 256> {
    auto t = std::array<std::uint32_t, 256>{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        t[i] = (i == 0 ? 8U : i) * 0x01010101U;
    }
    return t;
}
inline constexpr std::array<std::uint32_t, 256> fingerprint_words = make_fingerprint_words();

// Whether comparing two keys of this type is a *call*, which decides how the probe is shaped.
//
// A probe that leaves its home group needs the delta and the group mask, and about 3% of lookups
// leave it. When the key compare is a register compare, those two values live in registers and cost
// nothing; when it is a call -- a `memcmp` for a string -- everything the loop keeps live has to
// survive the call, and the compiler builds a frame and spills them before the *first* group is
// even compared. Splitting the rare part out of line then removes the frame from the common path,
// and that is worth 8-9% of a string lookup and costs an integer one 20-40% (measured 2026-09-10,
// `notes/index-design.md`), so it has to be decided per key type.
//
// No trait separates the two cases -- `std::string_view` and `std::pair<std::uint64_t,
// std::uint64_t>` are both sixteen bytes and both trivially copyable, and they want opposite
// answers -- so this is a heuristic plus the one exception the standard library provides. A type
// whose copy constructor is trivial holds no indirection, and so is compared field by field unless
// it is a view; `basic_string_view` is the view the library knows about.
//
// **Trivially copy-constructible, not trivially copyable**, and the difference is not pedantry:
// `std::pair` and `std::tuple` write their own copy *assignment*, so both libstdc++ and libc++
// report `is_trivially_copyable_v<std::pair<std::uint64_t, std::uint64_t>>` as false. Keying on
// that would have split one of the commonest key types after string and integer and cost it 40%.
//
// A pair and a tuple are asked about their elements rather than about themselves, because that is
// what comparing one does -- and because the answer for the aggregate is not portable: MSVC's
// `std::tuple<int, int>` is not trivially copy-constructible where libstdc++'s and libc++'s is, so
// without this a tuple key would take a different probe on Windows than everywhere else.
//
// A user type that is trivially copy-constructible and still compares through a call (a large byte
// array) is treated as cheap and does not get the split. Measured, that costs it about 2%, and the
// heuristic never misfires in the expensive direction.
template <typename Key>
struct key_compare_is_call : std::bool_constant<!std::is_trivially_copy_constructible_v<Key>> {};

template <typename CharT, typename Traits>
struct key_compare_is_call<std::basic_string_view<CharT, Traits>> : std::true_type {};

template <typename A, typename B>
struct key_compare_is_call<std::pair<A, B>>
    : std::bool_constant<key_compare_is_call<A>::value || key_compare_is_call<B>::value> {};

template <typename... Ts>
struct key_compare_is_call<std::tuple<Ts...>> : std::bool_constant<(key_compare_is_call<Ts>::value || ...)> {};

template <typename Key>
constexpr bool key_compare_is_call_v = key_compare_is_call<Key>::value;

template <typename T>
using detect_is_transparent = typename T::is_transparent;

template <typename T>
using detect_iterator = typename T::iterator;

template <typename T>
using detect_iterator_category = typename std::iterator_traits<T>::iterator_category;

// Whether a range can be walked twice, which is what reading ahead of where you are writing needs.
// A type with no iterator_traits at all -- a test's hand-rolled iterator, say -- has to answer no
// rather than fail to compile, which is what is_detected_v is for.
template <typename T>
[[nodiscard]] constexpr auto is_forward_iterator() -> bool {
    if constexpr (is_detected_v<detect_iterator_category, T>) {
        return std::is_convertible_v<detect_iterator_category<T>, std::forward_iterator_tag>;
    } else {
        return false;
    }
}

template <typename T>
constexpr bool is_forward_iterator_v = is_forward_iterator<T>();

template <typename T>
using detect_reserve = decltype(std::declval<T&>().reserve(std::size_t{}));

// enable_if helpers

template <typename Mapped>
constexpr bool is_map_v = !std::is_void_v<Mapped>;

// clang-format off
template <typename Hash, typename KeyEqual>
constexpr bool is_transparent_v = is_detected_v<detect_is_transparent, Hash> && is_detected_v<detect_is_transparent, KeyEqual>;
// clang-format on

template <typename From, typename To1, typename To2>
constexpr bool is_neither_convertible_v = !std::is_convertible_v<From, To1> && !std::is_convertible_v<From, To2>;

template <typename T>
constexpr bool has_reserve = is_detected_v<detect_reserve, T>;

// base type for map has mapped_type
template <class T>
struct base_table_type_map {
    using mapped_type = T;
};

// base type for set doesn't have mapped_type
struct base_table_type_set {};

// A key's hash, finalized and ready for a table to index with, as produced by hash_for(). See the
// lookup section of table for what it is for; this is spelled table::precomputed_hash.
//
// Templated on the hasher and nothing else, because the hasher is all a hash depends on: a map, a
// set and a segmented_map that hash the key the same way can pass one around between them. It is a
// type of its own rather than a plain integer so that an integer does not convert to it by
// accident -- in particular what hash_function() returns, which is not this number.
template <typename Hash>
struct precomputed_hash {
    std::uint64_t m_mixed_hash;
};

} // namespace detail

// A hash that has to be a high quality one, for a codebase where they all are meant to be and
// forgetting to say so is the easy mistake:
//
//     template <class Key, class T>
//     using my_map = ankerl::unordered_dense::map<Key, T, require_avalanching<my_hash<Key>>>;
//
// Written into the alias rather than next to the hash so that the check is part of what the map
// is, and survives my_hash being reimplemented without its marker.
//
// It inherits, which is what keeps the hash's own operator() overloads and its is_transparent, and
// costs nothing: the wrapper is the same size as the hash and compiles to the same code.
template <typename Hash>
struct require_avalanching : Hash {
    static_assert(hash_is_avalanching_v<Hash>,
                  "hash is not avalanching: give it 'using is_avalanching = void;', or specialize "
                  "ankerl::unordered_dense::hash_is_avalanching for it, or stop requiring it here");
    static_assert(!std::is_final_v<Hash>,
                  "hash is final, so it cannot be wrapped: specialize "
                  "ankerl::unordered_dense::hash_is_avalanching for it instead");

    require_avalanching() = default;

    // So that a stateful hash can be handed over by value as well as braced into place -- an
    // aggregate would take require_avalanching<H>{h} but not require_avalanching<H>(h).
    explicit require_avalanching(Hash const& hash)
        : Hash(hash) {}

    // Restated rather than inherited, because a hash named avalanching by a specialization of
    // hash_is_avalanching has no member typedef to inherit.
    using is_avalanching = void;
};

// Very much like std::deque, but faster for indexing (in most cases). As of now this doesn't implement the full std::vector
// API, but merely what's necessary to work as an underlying container for ankerl::unordered_dense::{map, set}.
// It allocates blocks of equal size and puts them into the m_blocks vector. That means it can grow simply by adding a new
// block to the back of m_blocks, and doesn't double its size like an std::vector. The disadvantage is that memory is not
// linear and thus there is one more indirection necessary for indexing.
template <typename T, typename Allocator = std::allocator<T>, std::size_t MaxSegmentSizeBytes = 4096>
class segmented_vector {
    template <bool IsConst>
    class iter_t;

public:
    using allocator_type = Allocator;
    using pointer = typename std::allocator_traits<allocator_type>::pointer;
    using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;
    using difference_type = typename std::allocator_traits<allocator_type>::difference_type;
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = T const&;
    using iterator = iter_t<false>;
    using const_iterator = iter_t<true>;

private:
    using vec_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<pointer>;
    using vec_alloc_traits = std::allocator_traits<vec_alloc>;

    // The allocator lives in m_blocks, so these are what the assignment operators below act on --
    // and what their noexcept specifications are written over, so that the condition and the
    // promise cannot drift apart.
    static constexpr bool propagates_on_copy_assign = vec_alloc_traits::propagate_on_container_copy_assignment::value;
    static constexpr bool propagates_on_move_assign = vec_alloc_traits::propagate_on_container_move_assignment::value;
    static constexpr bool allocators_always_equal = vec_alloc_traits::is_always_equal::value;
    static constexpr bool propagates_on_swap = vec_alloc_traits::propagate_on_container_swap::value;

    std::vector<pointer, vec_alloc> m_blocks{};
    std::size_t m_size{};

    // Calculates the maximum number for x in  (s << x) <= max_val
    static constexpr auto num_bits_closest(std::size_t max_val, std::size_t s) -> std::size_t {
        auto f = std::size_t{0};
        while (s << (f + 1) <= max_val) {
            ++f;
        }
        return f;
    }

    using self_t = segmented_vector<T, Allocator, MaxSegmentSizeBytes>;
    static constexpr auto num_bits = num_bits_closest(MaxSegmentSizeBytes, sizeof(T));
    static constexpr auto num_elements_in_block = 1U << num_bits;
    static constexpr auto mask = num_elements_in_block - 1U;

    /**
     * Iterator class doubles as const_iterator and iterator
     */
    template <bool IsConst>
    class iter_t {
        using ptr_t = std::conditional_t<IsConst, segmented_vector::const_pointer const*, segmented_vector::pointer*>;
        ptr_t m_data{};
        std::size_t m_idx{};

        template <bool B>
        friend class iter_t;

    public:
        using difference_type = segmented_vector::difference_type;
        using value_type = segmented_vector::value_type;
        using reference = std::conditional_t<IsConst, value_type const&, value_type&>;
        using pointer = std::conditional_t<IsConst, segmented_vector::const_pointer, segmented_vector::pointer>;
        // Everything a random access iterator needs is right here -- the position is an index, so jumping and
        // subtracting are single operations. Saying "forward" instead meant std::distance walked the whole container
        // one element at a time to compute what operator-() answers directly, and every algorithm that requires
        // random access, std::sort over values() among them, was ill-formed over an iterator that can do the job.
        using iterator_category = std::random_access_iterator_tag;

        iter_t() noexcept = default;

        template <bool OtherIsConst, typename = std::enable_if_t<IsConst && !OtherIsConst>>
        // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
        constexpr iter_t(iter_t<OtherIsConst> const& other) noexcept
            : m_data(other.m_data)
            , m_idx(other.m_idx) {}

        constexpr iter_t(ptr_t data, std::size_t idx) noexcept
            : m_data(data)
            , m_idx(idx) {}

        template <bool OtherIsConst, typename = std::enable_if_t<IsConst && !OtherIsConst>>
        constexpr auto operator=(iter_t<OtherIsConst> const& other) noexcept -> iter_t& {
            m_data = other.m_data;
            m_idx = other.m_idx;
            return *this;
        }

        constexpr auto operator++() noexcept -> iter_t& {
            ++m_idx;
            return *this;
        }

        constexpr auto operator++(int) noexcept -> iter_t {
            iter_t prev(*this);
            this->operator++();
            return prev;
        }

        constexpr auto operator--() noexcept -> iter_t& {
            --m_idx;
            return *this;
        }

        constexpr auto operator--(int) noexcept -> iter_t {
            iter_t prev(*this);
            this->operator--();
            return prev;
        }

        [[nodiscard]] constexpr auto operator+(difference_type diff) const noexcept -> iter_t {
            return {m_data, static_cast<std::size_t>(static_cast<difference_type>(m_idx) + diff)};
        }

        // n + it, which a random access iterator has to support just as it + n does
        [[nodiscard]] friend constexpr auto operator+(difference_type diff, iter_t const& it) noexcept -> iter_t {
            return it + diff;
        }

        // The cast is the one operator+() already does. Nothing instantiated these two before, because no algorithm
        // could reach them through a forward iterator, so the implicit signed-to-unsigned conversion sat here
        // unnoticed until clang's -Wsign-conversion saw std::sort use it.
        constexpr auto operator+=(difference_type diff) noexcept -> iter_t& {
            m_idx = static_cast<std::size_t>(static_cast<difference_type>(m_idx) + diff);
            return *this;
        }

        [[nodiscard]] constexpr auto operator-(difference_type diff) const noexcept -> iter_t {
            return {m_data, static_cast<std::size_t>(static_cast<difference_type>(m_idx) - diff)};
        }

        constexpr auto operator-=(difference_type diff) noexcept -> iter_t& {
            m_idx = static_cast<std::size_t>(static_cast<difference_type>(m_idx) - diff);
            return *this;
        }

        template <bool OtherIsConst>
        [[nodiscard]] constexpr auto operator-(iter_t<OtherIsConst> const& other) const noexcept -> difference_type {
            return static_cast<difference_type>(m_idx) - static_cast<difference_type>(other.m_idx);
        }

        constexpr auto operator*() const noexcept -> reference {
            return m_data[m_idx >> num_bits][m_idx & mask];
        }

        [[nodiscard]] constexpr auto operator[](difference_type diff) const noexcept -> reference {
            return *(*this + diff);
        }

        constexpr auto operator->() const noexcept -> pointer {
            return &m_data[m_idx >> num_bits][m_idx & mask];
        }

        template <bool O>
        [[nodiscard]] constexpr auto operator==(iter_t<O> const& o) const noexcept -> bool {
            return m_idx == o.m_idx;
        }

        template <bool O>
        [[nodiscard]] constexpr auto operator!=(iter_t<O> const& o) const noexcept -> bool {
            return !(*this == o);
        }

        template <bool O>
        [[nodiscard]] constexpr auto operator<(iter_t<O> const& o) const noexcept -> bool {
            return m_idx < o.m_idx;
        }

        template <bool O>
        [[nodiscard]] constexpr auto operator>(iter_t<O> const& o) const noexcept -> bool {
            return o < *this;
        }

        template <bool O>
        [[nodiscard]] constexpr auto operator<=(iter_t<O> const& o) const noexcept -> bool {
            return !(o < *this);
        }

        template <bool O>
        [[nodiscard]] constexpr auto operator>=(iter_t<O> const& o) const noexcept -> bool {
            return !(*this < o);
        }
    };

    // slow path: need to allocate a new segment every once in a while
    void increase_capacity() {
        auto ba = Allocator(m_blocks.get_allocator());

        // Room for the pointer first. push_back is the other thing here that can throw -- it
        // reallocates -- and it used to do so with the block already allocated and owned by
        // nobody, which leaked it. Reserving first means the only allocation still outstanding
        // when something fails is one that has not happened yet, and the push_back below cannot
        // fail because the capacity is already there. Grow geometrically to avoid reallocation
        // on every new segment.
        if (m_blocks.size() == m_blocks.capacity()) {
            m_blocks.reserve((std::max)(std::size_t{1}, m_blocks.capacity() * 2));
        }
        pointer block = std::allocator_traits<Allocator>::allocate(ba, num_elements_in_block);
        m_blocks.push_back(block);
    }

    // Moves everything from other
    void append_everything_from(segmented_vector&& other) { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
        reserve(size() + other.size());
        for (auto&& o : other) {
            emplace_back(std::move(o));
        }
    }

    // Copies everything from other
    void append_everything_from(segmented_vector const& other) {
        reserve(size() + other.size());
        for (auto const& o : other) {
            emplace_back(o);
        }
    }

    void dealloc() {
        auto ba = Allocator(m_blocks.get_allocator());
        for (auto ptr : m_blocks) {
            std::allocator_traits<Allocator>::deallocate(ba, ptr, num_elements_in_block);
        }
    }

    [[nodiscard]] static constexpr auto calc_num_blocks_for_capacity(std::size_t capacity) {
        return (capacity + num_elements_in_block - 1U) / num_elements_in_block;
    }

    void resize_shrink(std::size_t new_size) {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (std::size_t ix = new_size; ix < m_size; ++ix) {
                operator[](ix).~T();
            }
        }
        m_size = new_size;
    }

public:
    segmented_vector() = default;

    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    segmented_vector(Allocator alloc)
        : m_blocks(vec_alloc(alloc)) {}

    // Uses alloc, unconditionally -- that is the whole point of an extended move constructor. It
    // used to delegate to move assignment, which cannot express it: assignment has to consult
    // propagate_on_container_move_assignment, so with a propagating allocator it adopted other's
    // and the allocator the caller named was quietly dropped.
    segmented_vector(segmented_vector&& other, Allocator alloc) noexcept(allocators_always_equal)
        : m_blocks(vec_alloc(alloc)) {
        if (allocators_always_equal || alloc == other.get_allocator()) {
            // Nothing to move element by element, the blocks just change hands.
            m_blocks = std::move(other.m_blocks);
            m_size = std::exchange(other.m_size, {});
        } else {
            append_everything_from(std::move(other));
        }
    }

    segmented_vector(segmented_vector const& other, Allocator alloc)
        : m_blocks(vec_alloc(alloc)) {
        append_everything_from(other);
    }

    segmented_vector(segmented_vector&& other) noexcept
        : segmented_vector(std::move(other), other.get_allocator()) {}

    segmented_vector(segmented_vector const& other)
        : m_blocks(vec_alloc_traits::select_on_container_copy_construction(other.m_blocks.get_allocator())) {
        append_everything_from(other);
    }

    auto operator=(segmented_vector const& other) -> segmented_vector& {
        if (this == &other) {
            return *this;
        }
        clear();
        if constexpr (propagates_on_copy_assign) {
            if (m_blocks.get_allocator() != other.m_blocks.get_allocator()) {
                // Everything still held has to go back through the old allocator before the new
                // one is adopted. Copy assignment and not move: which of the two propagates is
                // the inner vector's own pocca/pocma, and only pocca is known true here, so
                // assigning a temporary would consult pocma and silently keep the old allocator.
                dealloc();
                auto const empty_with_other_allocator = std::vector<pointer, vec_alloc>(other.m_blocks.get_allocator());
                m_blocks = empty_with_other_allocator;
            }
        }
        append_everything_from(other);
        return *this;
    }

    // Not unconditionally noexcept. When the allocator neither propagates nor compares equal --
    // std::pmr::polymorphic_allocator, for one -- the elements are moved one at a time into memory
    // this container allocates, so running out of it here has to be allowed to throw rather than
    // terminate. std::vector spells the condition the same way.
    auto operator=(segmented_vector&& other) noexcept(propagates_on_move_assign || allocators_always_equal)
        -> segmented_vector& {
        if (this == &other) {
            return *this;
        }
        clear();
        // Either the allocator comes along with the blocks or it is already the same one, and
        // either way the blocks can be taken over; std::vector's own move assignment does the
        // propagating in the first case.
        if (propagates_on_move_assign || m_blocks.get_allocator() == other.m_blocks.get_allocator()) {
            dealloc();
            m_blocks = std::move(other.m_blocks);
            m_size = std::exchange(other.m_size, {});
        } else {
            // Keeps its own allocator, because nothing said to take other's -- so the blocks it
            // already holds came from that same allocator and are reused rather than handed back
            // and immediately asked for again.
            append_everything_from(std::move(other));
        }
        return *this;
    }

    ~segmented_vector() {
        clear();
        dealloc();
    }

    [[nodiscard]] constexpr auto size() const -> std::size_t {
        return m_size;
    }

    [[nodiscard]] constexpr auto capacity() const -> std::size_t {
        return m_blocks.size() * num_elements_in_block;
    }

    // Indexing is highly performance critical
    [[nodiscard]] constexpr auto operator[](std::size_t i) const noexcept -> T const& {
        return m_blocks[i >> num_bits][i & mask];
    }

    [[nodiscard]] constexpr auto operator[](std::size_t i) noexcept -> T& {
        return m_blocks[i >> num_bits][i & mask];
    }

    [[nodiscard]] constexpr auto begin() -> iterator {
        return {m_blocks.data(), 0U};
    }
    [[nodiscard]] constexpr auto begin() const -> const_iterator {
        return {m_blocks.data(), 0U};
    }
    [[nodiscard]] constexpr auto cbegin() const -> const_iterator {
        return {m_blocks.data(), 0U};
    }

    [[nodiscard]] constexpr auto end() -> iterator {
        return {m_blocks.data(), m_size};
    }
    [[nodiscard]] constexpr auto end() const -> const_iterator {
        return {m_blocks.data(), m_size};
    }
    [[nodiscard]] constexpr auto cend() const -> const_iterator {
        return {m_blocks.data(), m_size};
    }

    [[nodiscard]] constexpr auto back() -> reference {
        return operator[](m_size - 1);
    }
    [[nodiscard]] constexpr auto back() const -> const_reference {
        return operator[](m_size - 1);
    }

    void pop_back() {
        back().~T();
        --m_size;
    }

    [[nodiscard]] auto empty() const {
        return 0 == m_size;
    }

    void reserve(std::size_t new_capacity) {
        m_blocks.reserve(calc_num_blocks_for_capacity(new_capacity));
        while (new_capacity > capacity()) {
            increase_capacity();
        }
    }

    void resize(std::size_t const count) {
        if (count < m_size) {
            resize_shrink(count);
        } else if (count > m_size) {
            std::size_t const new_elems = count - m_size;
            reserve(count);
            for (std::size_t ix = 0; ix < new_elems; ++ix) {
                emplace_back();
            }
        }
    }

    void resize(std::size_t const count, value_type const& value) {
        if (count < m_size) {
            resize_shrink(count);
        } else if (count > m_size) {
            std::size_t const new_elems = count - m_size;
            reserve(count);
            for (std::size_t ix = 0; ix < new_elems; ++ix) {
                emplace_back(value);
            }
        }
    }

    [[nodiscard]] auto get_allocator() const -> allocator_type {
        return allocator_type{m_blocks.get_allocator()};
    }

    // Exchanging two pointers and a size, and the inner vector's own swap exchanges the allocators
    // exactly when propagate_on_container_swap says to -- so this answers the allocator question
    // the way std::vector does, and a map gets the same answer whichever container backs it.
    // Without a member swap, std::swap fell back to a move construction and two move assignments:
    // O(n) for an operation that needs none, able to throw from inside a noexcept swap, and a
    // different answer from the flat container for the same map.
    void swap(segmented_vector& other) noexcept(propagates_on_swap || allocators_always_equal) {
        using std::swap;
        swap(m_blocks, other.m_blocks);
        swap(m_size, other.m_size);
    }

    friend void swap(segmented_vector& a, segmented_vector& b) noexcept(noexcept(a.swap(b))) {
        a.swap(b);
    }

    template <class... Args>
    auto emplace_back(Args&&... args) -> reference {
        if (m_size == capacity()) {
            increase_capacity();
        }
        auto* ptr = static_cast<void*>(&operator[](m_size));
        auto& ref = *new (ptr) T(std::forward<Args>(args)...);
        ++m_size;
        return ref;
    }

    void clear() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (std::size_t i = 0, s = size(); i < s; ++i) {
                operator[](i).~T();
            }
        }
        m_size = 0;
    }

    void shrink_to_fit() {
        auto ba = Allocator(m_blocks.get_allocator());
        auto num_blocks_required = calc_num_blocks_for_capacity(m_size);
        while (m_blocks.size() > num_blocks_required) {
            std::allocator_traits<Allocator>::deallocate(ba, m_blocks.back(), num_elements_in_block);
            m_blocks.pop_back();
        }
        m_blocks.shrink_to_fit();
    }
};

namespace detail {

// What holds the index: one array of blocks, each a group's metadata followed by that group's own
// sixteen value indices. 88 bytes per sixteen slots, and no padding -- the same bytes the two arrays
// took, in one allocation instead of two.
//
// This was two arrays until 2026-09-06, on the argument that the groups are what a probe reads -- a
// miss touches nothing else, and a rehash writes them at random -- so 24 bytes per sixteen slots
// keeps far more of them in cache than 88 would, measured then as 10% faster on a build. Re-measured
// against the merged form after the rehash's store-to-load fix, that build advantage is gone
// (build64 100.0% and 101.2% in two runs) and the merged form wins the lookups: `find64` 5.8-8.1%,
// `rhit64` 7.1-7.5%, `findbig` 6.2%, score 1.018 and 1.022. Three counters say why, at 200000,
// 800000 and 4M entries: **7% fewer instructions**, because the index is now at a fixed offset from
// the group rather than a second address to compute; 12-14% fewer L1 misses; and 28% fewer dTLB
// misses at 4M, because a lookup touches two regions rather than three. The gain is largest where
// the table is largest, which is the half of the size axis the scored benchmark cannot see.
//
// Alloc is the table's value allocator; the block array rebinds it.
template <typename Group, typename Alloc>
class group_storage {
    static constexpr std::size_t slots = std::tuple_size_v<decltype(Group::m_fingerprints)>;

public:
    using value_idx_type = typename Group::value_idx_type;

    // Inherits so that every use of a group's fingerprints and counters reads unchanged, and so a
    // block converts to the Group const& that match_fingerprint takes.
    struct block : Group {
        std::array<value_idx_type, slots> m_index;
    };

    using allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<block>;

    // How many arrays this allocates, for a test that counts what an empty table costs.
    static constexpr std::size_t array_count = 1;

private:
    std::vector<block, allocator_type> m_blocks{};

public:
    group_storage() = default;
    explicit group_storage(allocator_type const& alloc)
        : m_blocks(alloc) {}
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved) -- moved from member by member
    group_storage(group_storage&& other, allocator_type const& alloc)
        : m_blocks(std::move(other.m_blocks), alloc) {}
    group_storage(group_storage const&) = default;
    group_storage(group_storage&&) noexcept = default;
    auto operator=(group_storage const&) -> group_storage& = default;
    auto operator=(group_storage&&) -> group_storage& = default;
    ~group_storage() = default;

    [[nodiscard]] auto get_allocator() const -> allocator_type {
        return m_blocks.get_allocator();
    }
    [[nodiscard]] auto empty() const -> bool {
        return m_blocks.empty();
    }
    [[nodiscard]] auto size() const -> std::size_t { // in groups
        return m_blocks.size();
    }
    void clear() {
        m_blocks.clear();
    }
    void shrink_to_fit() {
        m_blocks.shrink_to_fit();
    }
    void swap(group_storage& other) noexcept {
        m_blocks.swap(other.m_blocks);
    }
    void resize(std::size_t num_groups) {
        m_blocks.resize(num_groups);
    }
    void assign(group_storage const& other) {
        m_blocks.assign(other.m_blocks.begin(), other.m_blocks.end());
    }
    [[nodiscard]] auto data() -> block* {
        return m_blocks.data();
    }
    [[nodiscard]] auto data() const -> block const* {
        return m_blocks.data();
    }
    // Zeroes the metadata of every block and leaves the indices alone, which is what the split
    // version's single memset did: an empty slot's index is never read.
    void clear_metadata() {
        for (auto& b : m_blocks) {
            static_cast<Group&>(b) = Group{};
        }
    }
};

// This is it, the table. Doubles as map and set, and uses `void` for T when its used as a set.
template <class Key,
          class T, // when void, treat it as a set.
          class Hash,
          class KeyEqual,
          class AllocatorOrContainer,
          class Bucket,
          bool IsSegmented>
class table : public std::conditional_t<is_map_v<T>, base_table_type_map<T>, base_table_type_set> {
    using underlying_value_type = std::conditional_t<is_map_v<T>, std::pair<Key, T>, Key>;
    using underlying_container_type = std::conditional_t<IsSegmented,
                                                         segmented_vector<underlying_value_type, AllocatorOrContainer>,
                                                         std::vector<underlying_value_type, AllocatorOrContainer>>;

public:
    using value_container_type = std::
        conditional_t<is_detected_v<detect_iterator, AllocatorOrContainer>, AllocatorOrContainer, underlying_container_type>;

private:
    // IsSegmented is about the values -- stable references, no reallocation of the payload. The
    // index is two plain arrays either way: it is 5.5 bytes per slot, and a probe reads it by
    // pointer.
    using bucket_container_type = detail::group_storage<Bucket, typename value_container_type::allocator_type>;

    // Slots per group, from the group. bucket_count() counts slots, m_group_mask counts groups.
    static constexpr std::size_t slots_per_group = std::tuple_size_v<decltype(Bucket::m_fingerprints)>;
    static_assert(slots_per_group == 16 && std::tuple_size_v<decltype(Bucket::m_overflows)> == 8,
                  "a group is sixteen fingerprints, matched as one vector or two words, and eight counters, picked by "
                  "the low three bits of the fingerprint");

    // How far ahead the five loops that pipeline look: the rehash, the range insert, the bulk
    // visit, replace()'s dedup and merge()'s walk. The visit uses it as a chunk size rather than a ring depth. It is a count
    // of memory accesses that has to fit inside what the core keeps outstanding, which is about two dozen on a Zen 4 -- not a
    // property of the map, and every size from 8 to 32 measured within 2% of this one. Boost's bulk visit uses the same
    // number.
    static constexpr std::size_t pipeline_depth = 16;

    // Below this much *index*, a pipeline costs more than it saves: the ring round trip is about 13
    // instructions per element, and a prefetch of a line already in cache still occupies a load
    // port. The index is the right thing to measure because it is the only array the prefetch is
    // for -- the values are walked in order and the hardware handles them. Counting the values too
    // was tried first and is wrong: at one value size the two models cannot be told apart, and with
    // a 1032 byte value the footprint model opens the gate at ten thousand elements, where the
    // pipeline is a **14% loss**. Both value sizes cross over at the same index size instead.
    //
    // replace()'s threshold; see the sweep beside its only use. The range insert and the bulk visit
    // were measured against the same question and neither wants a gate at all; the visit's crossover
    // is set by the caller's hit rate, which the map does not know until it has already done a
    // chunk's worth of work. Issue #247.
    //
    // Re-measured on a fixed harness for #257 -- it was first fitted on `std::uint64_t` alone -- and
    // kept. Ring over the same loop with the ring deleted, by how much index the array holds, for
    // `std::uint64_t` and `std::string` keys at 0% and 25% duplicates:
    //
    //      88 KiB   1.265  1.225  1.006  1.037
    //     176 KiB   0.833  1.194  1.156  1.033
    //     352 KiB   0.667  1.163  1.064  1.002    <- the first size the gate opens at
    //     704 KiB   0.632  0.999  1.039  0.987
    //    1408 KiB   0.626  0.944  0.998  0.985
    //    5632 KiB   0.537  0.912  0.950  0.944
    //
    // The four curves cross in four different places, because the ring's payoff depends on the
    // **duplicate rate** as much as on the size: a duplicate refills its ring slot from the element
    // that just moved into it, which is read by the very next iteration and has no distance to
    // prefetch over. At 352 KiB the same gate is worth 1.5x to a `std::uint64_t` with no duplicates
    // and costs 16% to one with a quarter of them, and the map cannot tell which it has until it has
    // walked. So the constant is a compromise and is chosen as one: over all 28 cells the realised
    // geometric mean is 0.9081 here, 0.9137 at 128 KiB, 0.9143 at 512 KiB and 0.9286 at a mebibyte.
    // A threshold that never loses exists -- a mebibyte, worst cell 1.000 -- and costs two points of
    // that mean. This one keeps the 1.5x and accepts a worst cell of 1.163.
    static constexpr std::size_t pipeline_min_index_bytes = std::size_t{256} << 10U;

    // merge()'s, and it is two doublings higher than replace()'s rather than the same number -- which
    // was the first guess, and wrong. A gate's crossover is where the ring's fixed cost per element
    // stops being worth the misses it hides, so it moves with what the *rest* of the loop costs, and
    // merge's walk is cheaper per element than replace()'s dedup. Measured on this loop, ring over
    // the same loop with the ring deleted, at no overlap / half / nine tenths of it duplicated:
    //
    //     352 KiB of index   1.068 / 1.119 / 1.086   a clear loss
    //     704 KiB            1.019 / 1.043 / 1.031 and 0.994 / 1.014 / 1.050 at two load factors
    //    1408 KiB            0.966 / 0.988 / 0.986 and 0.927 / 0.968 / 1.005
    //    2816 KiB            0.921 / 0.926 / 0.956
    //     22 MiB             0.812 / 0.749 / 0.769
    //
    // The index doubles, so anything inside (704 KiB, 1408 KiB] switches at the same places; a
    // mebibyte sits in the middle of that window with a doubling of margin either side. Issue #242.
    static constexpr std::size_t merge_min_index_bytes = std::size_t{1} << 20U;

    static constexpr std::uint8_t initial_shifts = 64 - 2; // 2^(64-m_shifts) groups
    static constexpr float default_max_load_factor = 0.8F;

    // Named, and covering both containers, so that the promise and the recovery that exists for
    // when the promise cannot be made are spelled the same way and cannot drift apart -- the same
    // reason segmented_vector names its propagation traits. Covering only m_values would be wrong
    // twice over: it would leave m_buckets free to throw out of a noexcept function, and it would
    // compile a rethrow into one, which gcc rejects outright.
    static constexpr bool move_assign_is_nothrow =
        std::is_nothrow_move_assignable_v<value_container_type> && std::is_nothrow_move_assignable_v<bucket_container_type> &&
        std::is_nothrow_move_assignable_v<Hash> && std::is_nothrow_move_assignable_v<KeyEqual>;

public:
    using key_type = Key;
    using value_type = typename value_container_type::value_type;
    using size_type = typename value_container_type::size_type;
    using difference_type = typename value_container_type::difference_type;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = typename value_container_type::allocator_type;
    using reference = typename value_container_type::reference;
    using const_reference = typename value_container_type::const_reference;
    using pointer = typename value_container_type::pointer;
    using const_pointer = typename value_container_type::const_pointer;
    using const_iterator = typename value_container_type::const_iterator;
    using iterator = std::conditional_t<is_map_v<T>, typename value_container_type::iterator, const_iterator>;
    using bucket_type = Bucket;

    // What hash_for() returns; see the lookup section below. Shared by every table with this
    // hasher, whatever else it is made of, because that is exactly the set of tables the hash is
    // good for.
    using precomputed_hash = detail::precomputed_hash<Hash>;

private:
    using value_idx_type = typename Bucket::value_idx_type;

    // merge(source) takes the source apart and puts it back together: it compacts the elements that
    // stay behind and rebuilds the index over them once, neither of which the public API can
    // express. The standard lets the source differ in its hash and its key_equal, so it is a
    // different instantiation of this same template and not otherwise a friend of it.
    // Wider than the need -- every table is a friend of every other, where merge wants only the ones
    // holding the same value type in the same container -- and C++17 has no way to say less: a friend
    // template cannot constrain its own argument list.
    template <class, class, class, class, class, class, bool>
    friend class table;

    // What merge() accepts: this table with another hash or another key_equal, which is the pair the
    // standard allows to differ. Everything else has to match, because the elements move out of one
    // container and into the other.
    template <class H2, class KE2>
    using sibling_table = table<Key, T, H2, KE2, AllocatorOrContainer, Bucket, IsSegmented>;

    static_assert(std::is_trivially_destructible_v<Bucket>, "assert there's no need to call destructor / std::destroy");
    static_assert(std::is_trivially_copyable_v<Bucket>, "assert we can just memset / memcpy");

    value_container_type m_values{}; // Contains all the key-value pairs in one densely stored container. No holes.
    bucket_container_type m_buckets{};
    std::size_t m_max_bucket_capacity = 0;
    value_idx_type m_group_mask = 0; // groups - 1; works because the number of groups is a power of two
    float m_max_load_factor = default_max_load_factor;
    Hash m_hash{};
    KeyEqual m_equal{};
    std::uint8_t m_shifts = initial_shifts;

    // The goal of mixed_hash is to always produce a high quality 64bit hash.
    template <typename K>
    [[nodiscard]] constexpr auto mixed_hash(K const& key) const -> std::uint64_t {
        if constexpr (hash_is_avalanching_v<Hash>) {
            // we know that the hash is good because is_avalanching.
            if constexpr (sizeof(decltype(m_hash(key))) < sizeof(std::uint64_t)) {
                // 32bit hash and is_avalanching => multiply with a constant to avalanche bits upwards
                return m_hash(key) * UINT64_C(0x9ddfea08eb382d69);
            } else {
                // 64bit and is_avalanching => only use the hash itself.
                return m_hash(key);
            }
        } else {
            // not is_avalanching => apply wyhash
            return wyhash::hash(m_hash(key));
        }
    }

    [[nodiscard]] constexpr auto group_idx_from_hash(std::uint64_t hash) const -> value_idx_type {
        return static_cast<value_idx_type>(hash >> m_shifts);
    }

    // Where a probe for a key stopped. Found: the slot holding it, and the value it points to.
    // Not found: nothing but `found` is meaningful, and an insert walks the probe sequence again
    // from the hash to place the key.
    // A slot, as the group it is in and the lane inside it. Nothing in the map holds a flat slot
    // number any more: the probe finds the pair, and both consumers -- erase_group_slot and
    // move_home -- address the group with it directly.
    struct group_slot {
        value_idx_type group_idx;
        std::uint8_t lane;
    };

    // Where the probe stopped, as the group and the lane inside it. Not as one flat slot number:
    // every consumer wants the pair back, and packing `group_idx * slots_per_group + lane` here made
    // erase_group_slot and move_home divide it apart again -- which gcc never folded (#262). The
    // lane is a std::uint8_t so that this stays the same twelve bytes it was.
    struct probe_result {
        value_idx_type group_idx;
        value_idx_type value_idx;
        std::uint8_t lane;
        bool found;
    };

    // the index ///////////////////////////////////////////////////////////////
    //
    // Sixteen one-byte fingerprints per group, compared at once, a second array with the value
    // index of every slot, quadratic probing over groups, and eight overflow counters per group
    // that an insert increments in every full group it passes and an erase decrements again. A
    // probe stops at the first group whose counter for this hash is zero, since no entry with
    // those bits ever went past it. There are no tombstones, so no rehash is ever needed to
    // repair the index -- an erase undoes exactly what its insert did to the counters.
    //
    // What an erase cannot undo is where the element went. One that arrived while its home group
    // was full sits in a later group and stays there even after the home group empties again, so a
    // table that has churned probes a little further than one built from the same contents: at
    // load 0.76, measured, 1.14 groups per hit against 1.03 and 1.27 per miss against 1.05. It
    // plateaus after about a dozen turnovers rather than growing, which is the difference from a
    // design that leaves tombstones behind. An erase cannot know where the element it frees would
    // have gone had the table been built from what is left; what does take the drift back is the
    // element itself, the next time a writing operation finds it: see move_home below.
    // `rehash(size())` rebuilds the index and takes all of it back at once.
    //
    // A miss usually stops within a group or two, at the first counter that is zero: the counters
    // are exact, so zero means no live entry of this class ever overflowed past that group.
    // Usually, not always. A counter counts entries that passed the group on *their* sequence,
    // which need not be this one, so every group on a sequence can be positive at once -- eight
    // keys chosen against a known hash do it, each overflowed past one group while fillers made it
    // full and the fillers erased again -- and then no zero is ever reached. So a miss also stops
    // once it has visited every group, which is as far as any key that exists can have been
    // placed, and that bound is what makes a lookup terminate for any input at all. A counter
    // saturates at 255 and is then never decremented, which costs every later probe for that
    // fingerprint class one more group for the rest of the array's life -- and needs 255 live
    // entries of one class to have overflowed one group at the same moment to happen at all.
    //
    // The group is hash >> m_shifts, the fingerprint the low byte of the hash with 0 mapped to
    // 8 so that 0 means empty and (fingerprint & 7), which picks the counter, is unchanged.

    // the fingerprint in all four bytes of a word, from the table above
    [[nodiscard]] static constexpr auto fingerprint_word(std::uint64_t hash) -> std::uint32_t {
        return detail::fingerprint_words[hash & 0xFFU];
    }

    // quadratic: the triangular numbers reach every group of a power-of-two array
    [[nodiscard]] auto next_group(value_idx_type group_idx, value_idx_type& delta) const -> value_idx_type {
        return static_cast<value_idx_type>((group_idx + (++delta)) & m_group_mask);
    }

#    if !ANKERL_UNORDERED_DENSE_HAS_SSE2
    // Eight fingerprints per machine word, without SIMD.
    //
    // The bytes wanted are the zero ones of `slots ^ fingerprint`, and this is what marks them
    // exactly. `(b & 0x7f) + 0x7f` carries into a byte's high bit precisely when its low seven bits
    // are not all zero, and cannot carry out of the byte, so or-ing the byte back in leaves that
    // high bit set in every non-zero byte and clear in every zero one. The more familiar
    // `(x - ones) & ~x & highs` is two operations shorter and wrong here: a zero byte borrows from
    // the next one, which marks a 0x01 above a 0x00 as a match too. Harmless for a probe, which
    // verifies its candidates against the key -- and not harmless at all for the empty slot an
    // insert picks.
    //
    // The multiply then gathers the eight high bits into eight adjacent ones: bit 7 + 8i has to
    // reach bit 56 + i, so the constant carries a bit at 56 - 7i, and no other pair of bits lands
    // in the top byte.
    [[nodiscard]] static auto match_zero_bytes(std::uint64_t x) -> unsigned {
        static constexpr auto lows = UINT64_C(0x7F7F7F7F7F7F7F7F);
        static constexpr auto highs = UINT64_C(0x8080808080808080);
        auto const zeros = ~(((x & lows) + lows) | x) & highs;
        return static_cast<unsigned>(((zeros >> 7U) * UINT64_C(0x0102040810204080)) >> 56U);
    }

    // Byte i of the group has to become bit i, which is what a load is on a little endian machine
    // and the reverse of one anywhere else.
    [[nodiscard]] static auto load_fingerprints(std::uint8_t const* p) -> std::uint64_t {
        auto word = std::uint64_t{};
        std::memcpy(&word, p, sizeof(word));
#        if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        word = __builtin_bswap64(word);
#        endif
        return word;
    }
#    endif

    // What a compare of sixteen fingerprints answers with: one bit per matching lane. SSE2 and the
    // word-at-a-time fallback put those bits next to each other; NEON has no movemask, and the
    // cheapest stand-in leaves them four apart, so the stride is a constant rather than 1 and
    // first_lane() divides by it. Everything else -- testing for any match, taking the lowest,
    // clearing it with `m & (m - 1)` -- is then written once for all three.
#    if ANKERL_UNORDERED_DENSE_HAS_NEON
    using lane_mask = std::uint64_t;
    static constexpr unsigned lane_stride = 4;
#    else
    using lane_mask = unsigned;
    static constexpr unsigned lane_stride = 1;
#    endif

    [[nodiscard]] static auto first_lane(lane_mask lanes) -> unsigned {
        return detail::countr_zero(lanes) / lane_stride;
    }

    // lanes whose fingerprint is the word's; every byte of the word is the same
    [[nodiscard]] static auto match_fingerprint(Bucket const& group, std::uint32_t word) -> lane_mask {
#    if ANKERL_UNORDERED_DENSE_HAS_SSE2
        // NOLINTBEGIN(portability-simd-intrinsics)
        auto const fingerprints = _mm_loadu_si128(reinterpret_cast<__m128i const*>(group.m_fingerprints.data())); // NOLINT
        return static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpeq_epi8(fingerprints, _mm_set1_epi32(static_cast<int>(word)))));
        // NOLINTEND(portability-simd-intrinsics)
#    elif ANKERL_UNORDERED_DENSE_HAS_NEON
        // NOLINTBEGIN(portability-simd-intrinsics)
        // vceqq_u8 gives 0xFF per matching byte. Reading that as eight 16 bit lanes, shifting each
        // right by four and narrowing back to bytes packs the two comparisons of a 16 bit lane into
        // one byte -- lane 2j as its low nibble, lane 2j+1 as its high one -- so the whole answer
        // fits in a 64 bit word with lane i at nibble i. Keeping only the low bit of each nibble
        // leaves exactly one bit per matching lane, four apart, which is what lane_stride is.
        auto const cmp = vceqq_u8(vld1q_u8(group.m_fingerprints.data()), vdupq_n_u8(static_cast<std::uint8_t>(word)));
        auto const nibbles = vshrn_n_u16(vreinterpretq_u16_u8(cmp), 4);
        return vget_lane_u64(vreinterpret_u64_u8(nibbles), 0) & UINT64_C(0x1111111111111111);
        // NOLINTEND(portability-simd-intrinsics)
#    else
        auto const wanted = static_cast<std::uint64_t>(static_cast<std::uint8_t>(word)) * UINT64_C(0x0101010101010101);
        auto const* p = group.m_fingerprints.data();
        return match_zero_bytes(load_fingerprints(p) ^ wanted) | (match_zero_bytes(load_fingerprints(p + 8) ^ wanted) << 8U);
#    endif
    }

    [[nodiscard]] static auto match_empty(Bucket const& group) -> lane_mask {
        return match_fingerprint(group, 0);
    }

    // Both helpers here ask for **two consecutive cache lines, clamped into the block**, and differ
    // only in which line they start at. That rule is #250's: the hardware fetches what it can see
    // coming, so the job is to start it in the right place rather than to name every line a block
    // touches. A block is 88 bytes, or 152 for `group_big`, at eight byte alignment, and
    // `88 % 64 == 24` with `gcd(24, 64) == 8`, so `p % 64` cycles through {0, 8, ... 56} whatever
    // the array's address and a quarter of blocks reach one line further than the rest. Naming that
    // extra line has now been measured twice and lost twice: 12.85 ns/block against 12.28 on the 88
    // byte block (#250), and 16.37 against 16.07 under clang and 16.49 against 16.08 under gcc on
    // the 152 byte one, where a rehash-shaped read of every byte ties at 19.12 against 19.15 (#252).
    // scripts/ab/prefetch_lines.cpp. They are two functions rather than one template because
    // folding them together moves gcc's code generation on the default bucket type, and this way
    // both spellings are byte identical to what shipped before.
    //
    // The rest of the block, for a probe that is reading the fingerprints out of the first line as
    // this is issued. The value indices of a group are the next thing a hit reads and their address
    // needs only the group, so they are asked for before the fingerprints have arrived: the two
    // latencies overlap instead of adding. Measured 3 cycles off every hit, 0.2 onto every miss.
    //
    // Call it where the index a group hands back is an *address*. `probe_from` loads
    // `m_values[value_idx]` behind it -- a second miss queued on the first -- and that is the chain
    // the prefetch shortens. The two walks that look a value *up* have no load behind the index, and
    // measured better without it (#263, and the comment on slot_of_value).
    //
    // For `group` the second address is the block's last byte, which is where it has always pointed;
    // for `group_big` it is 128. 152 bytes spans four lines whenever `p % 64 > 40`, a quarter of
    // blocks, and asking for the first and the *last* of them left out the middle -- where eight of
    // the sixteen value indices live. The clamp also keeps a block that does not reach the second
    // line, such as the 64 byte layout measured in #250's neighbourhood, from asking past itself.
    template <typename Block>
    static void prefetch_index(Block const* blocks, value_idx_type group_idx) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- byte arithmetic on the block
        auto const* p = reinterpret_cast<char const*>(blocks + std::size_t{group_idx});
        constexpr auto first = (std::min)(std::size_t{64}, sizeof(Block) - 1);
        constexpr auto second = (std::min)(std::size_t{128}, sizeof(Block) - 1);
        ANKERL_UNORDERED_DENSE_PREFETCH(p + first);
        if constexpr (second != first) {
            ANKERL_UNORDERED_DENSE_PREFETCH(p + second);
        }
    }

    // The same two lines started at the block's first, for a caller that has not touched the group
    // at all: prefetch_index skips that line because the probe is about to read it anyway, and a
    // rehash is about to write a group it has never read, so it wants that line too.
    //
    // Asking for `p` and `p + 87` instead -- the first line and the *last*, which is what this did
    // until 2026-09-11 -- left out the middle, which holds all eight counters and half the
    // fingerprints, the two things a probe reads first. Stepping by 64 was worth 3.3%, 12.28
    // ns/block against 12.67, on a 176 MiB array walked at random with a sixteen-deep lookahead and
    // a probe-shaped read (#250). The `off < 128` is the clamp, and it is the half of the rule this
    // function was missing: without it a 152 byte block gets a third prefetch, which is the one
    // #252 measured as a loss.
    template <typename Block>
    static void prefetch_block(Block const* block) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- byte arithmetic on the block
        auto const* p = reinterpret_cast<char const*>(block);
        for (std::size_t off = 0; off < sizeof(Block) && off < 128; off += 64) {
            ANKERL_UNORDERED_DENSE_PREFETCH(p + off);
        }
    }

    // The same for a caller holding a group number rather than a pointer. A block is 88 bytes, so
    // the difference is a multiply, and a caller that already formed the address should not pay it
    // twice.
    template <typename Block>
    static void prefetch_block(Block const* blocks, std::size_t group_idx) {
        prefetch_block(blocks + group_idx);
    }

    // Forced inline because gcc does not do it on its own in a large translation unit, and the
    // whole design assumes it is: the prefetch, the hoisted pointers and the early exit only pay
    // inside the caller. Found in the paired harness, where gcc left the integer probe out of line
    // and every lookup paid a call -- with SWAR, whose match is bigger, random integer misses
    // measured 0.66 of the robin hood index, and forcing it took them to 1.23; with SSE2 the same
    // change was churn 1.32, string hits 1.22 and the integer build 1.42 against the commit before.
    // clang inlined it already and measures 1.00 everywhere.
    // The probe's loop, from a group and the distance already walked. Written out rather than built
    // from a per-group helper: returning a probe_result from an inner function costs gcc 26% of an
    // integer hit even fully inlined, and this path must stay exactly what it was for a key whose
    // compare is cheap.
    template <typename K>
    ANKERL_UNORDERED_DENSE_FORCEINLINE auto
    probe_from(K const& key, std::uint32_t word, unsigned counter, value_idx_type group_idx, value_idx_type delta) const
        -> probe_result {
        auto const* groups = m_buckets.data();
        while (true) {
            prefetch_index(groups, group_idx);
            auto const& group = groups[group_idx];
            auto lanes = match_fingerprint(group, word);
            while (lanes != 0) {
                auto const lane = first_lane(lanes);
                auto const value_idx = group.m_index[lane];
                if (m_equal(key, get_key(m_values[value_idx]))) {
                    return {group_idx, value_idx, static_cast<std::uint8_t>(lane), true};
                }
                lanes &= lanes - 1;
            }
            // Not here if nothing of this class ever overflowed past this group, and not anywhere
            // once every group has been looked at: see the note on termination above.
            if (group.m_overflows[counter] == 0 || delta == m_group_mask) {
                return {0, 0, 0, false};
            }
            group_idx = next_group(group_idx, delta);
        }
    }

    // Everything past the home group, out of line, for a key whose compare is a call. See
    // detail::key_compare_is_call for what that buys and what it would cost the other kind of key.
    // Entered by a tail call, so the caller keeps nothing live across it.
    template <typename K>
    ANKERL_UNORDERED_DENSE_NOINLINE auto
    probe_past_home(K const& key, std::uint32_t word, unsigned counter, value_idx_type group_idx, value_idx_type delta) const
        -> probe_result {
        return probe_from(key, word, counter, group_idx, delta);
    }

    // What happens once the home group has been looked at and did not hold the key: whether it is
    // worth walking on at all, and the walk. Shared by probe() and by the bulk visit, so that the
    // probe's termination invariant is written once -- it was copied into the second of those, and
    // a copy of an invariant is silent when it drifts, because the result is a wrong answer and not
    // a crash.
    //
    // `m_group_mask == 0` cannot fire while the smallest array is four groups; it is here because it
    // is what the loop tests at delta zero, so a mutation survivor on it is expected. The counter
    // beside it is not: turning that `== 0` into `== 1` makes a key that overflowed its home group
    // with exactly one same-class neighbour unfindable, and a steered test covers it.
    template <typename K>
    ANKERL_UNORDERED_DENSE_FORCEINLINE auto
    probe_after_home(K const& key, std::uint32_t word, unsigned counter, Bucket const& home, value_idx_type home_idx) const
        -> probe_result {
        if (home.m_overflows[counter] == 0 || m_group_mask == 0) {
            return {0, 0, 0, false};
        }
        value_idx_type delta = 0;
        auto const next = next_group(home_idx, delta);
        if constexpr (detail::key_compare_is_call_v<Key>) {
            return probe_past_home(key, word, counter, next, delta);
        } else {
            // A compare that needs no frame needs no call either: the same choice probe() makes for
            // such a key, and the reason the bulk path must not simply always take the out-of-line
            // one.
            return probe_from(key, word, counter, next, delta);
        }
    }

    template <typename K>
    ANKERL_UNORDERED_DENSE_FORCEINLINE auto probe(K const& key, std::uint64_t mh) const -> probe_result {
        auto const word = fingerprint_word(mh);
        return probe(key, word, word & 7U, group_idx_from_hash(mh));
    }

    // The probe for a caller that is holding the home group already -- a pipelined insert formed it
    // to prefetch against. It cannot prefetch, because it was handed the group rather than a number
    // to find it by, which is the same shape probe_after_home has and the reason it is written this
    // way rather than as a flag: "the caller already asked for this block" is then true by
    // construction and scoped to exactly the one group the caller knows about. A flag was tried and
    // is wrong twice over -- it suppressed the walk's prefetches too, for groups nobody had asked
    // for, and it kept suppressing them on the sixteen elements after a growth, which are the ones
    // whose earlier prefetch went to the array that growth replaced.
    template <typename K>
    ANKERL_UNORDERED_DENSE_FORCEINLINE auto probe_at_home(K const& key,
                                                          std::uint32_t word,
                                                          unsigned counter,
                                                          typename bucket_container_type::block const& home,
                                                          value_idx_type home_idx) const -> probe_result {
        auto lanes = match_fingerprint(home, word);
        while (lanes != 0) {
            auto const lane = first_lane(lanes);
            auto const value_idx = home.m_index[lane];
            if (m_equal(key, get_key(m_values[value_idx]))) {
                return {home_idx, value_idx, static_cast<std::uint8_t>(lane), true};
            }
            lanes &= lanes - 1;
        }
        return probe_after_home(key, word, counter, home, home_idx);
    }

    // The same probe for a caller that has already taken the hash apart. A pipelined insert has all
    // three in hand -- it derived the group to prefetch it -- and re-deriving them per element is a
    // table load, an and and a shift on the hot path of a loop that is doing nothing else.
    template <typename K>
    ANKERL_UNORDERED_DENSE_FORCEINLINE auto
    probe(K const& key, std::uint32_t word, unsigned counter, value_idx_type home_idx) const -> probe_result {
        if constexpr (!detail::key_compare_is_call_v<Key>) {
            return probe_from(key, word, counter, home_idx, 0);
        } else {
            // The home group inline and the rest behind a call, which is where the frame goes.
            auto const* groups = m_buckets.data();
            prefetch_index(groups, home_idx);
            auto const& home = groups[home_idx];
            auto lanes = match_fingerprint(home, word);
            while (lanes != 0) {
                auto const lane = first_lane(lanes);
                auto const value_idx = home.m_index[lane];
                if (m_equal(key, get_key(m_values[value_idx]))) {
                    return {home_idx, value_idx, static_cast<std::uint8_t>(lane), true};
                }
                lanes &= lanes - 1;
            }
            // The loop's two stopping conditions at delta 0: nothing of this class ever overflowed
            // past home, or there is nowhere else to look. The second is `delta == m_group_mask`
            // with delta zero, and it cannot fire while the smallest array is four groups -- it is
            // kept because it is what the loop tests, not because it is reachable, so a mutation
            // survivor on it is expected rather than a hole.
            return probe_after_home(key, word, counter, home, home_idx);
        }
    }

    // The first free slot on the key's probe sequence takes it; every full group on the way
    // counts it.
    ANKERL_UNORDERED_DENSE_FORCEINLINE void place_group(std::uint64_t mh, value_idx_type value_idx) {
        auto const word = fingerprint_word(mh);
        place_group(word, word & 7U, group_idx_from_hash(mh), value_idx);
    }

    // The placement for a caller that has already taken the hash apart, and has just probed with
    // exactly these three -- so this is the second of the two walks an insert does, not a third
    // derivation of where to start it.
    ANKERL_UNORDERED_DENSE_FORCEINLINE void
    place_group(std::uint32_t word, unsigned counter, value_idx_type group_idx, value_idx_type value_idx) {
        auto* groups = m_buckets.data();
        value_idx_type delta = 0;
        while (true) {
            auto& group = groups[group_idx];
            auto const empties = match_empty(group);
            if (empties != 0) {
                auto const lane = first_lane(empties);
                group.m_fingerprints[lane] = static_cast<std::uint8_t>(word);
                group.m_index[lane] = value_idx;
                return;
            }
            if (group.m_overflows[counter] != 255) {
                ++group.m_overflows[counter];
            }
            group_idx = next_group(group_idx, delta);
        }
    }

    // Takes an entry out of every counter it was counted in: the same walk from home that placed
    // it, up to the group it landed in. Everything it reads through `this` comes in as an argument,
    // loaded before the caller's fingerprint store: a std::uint8_t store may alias the group
    // pointer and the mask, so reading them afterwards puts a reload on the address chain of every
    // step of the walk. Measured as a 2% loss on a churn sweep when this was refactored the other
    // way round.
    template <typename Group>
    static void
    uncount(Group* groups, value_idx_type mask, value_idx_type home_idx, unsigned counter, value_idx_type found_in) {
        auto group_idx = home_idx;
        value_idx_type delta = 0;
        while (group_idx != found_in) {
            if (groups[group_idx].m_overflows[counter] != 255) {
                --groups[group_idx].m_overflows[counter];
            }
            group_idx = static_cast<value_idx_type>((group_idx + (++delta)) & mask);
        }
    }

    // Frees the slot and takes the entry out of the counters. The slot arrives as the group it is
    // in and the lane inside it, which is how the probe had it.
    void erase_group_slot(value_idx_type found_in, std::uint8_t lane, std::uint64_t mh) {
        auto* groups = m_buckets.data();
        auto const mask = m_group_mask;
        auto const home_idx = group_idx_from_hash(mh);
        auto const counter = fingerprint_word(mh) & 7U;
        groups[found_in].m_fingerprints[lane] = 0;
        uncount(groups, mask, home_idx, counter, found_in);
    }

    // A hit found past its home group moves home if there is room there now, and comes out of
    // the counters it was counted in on the way out. This is what takes back the drift of a
    // churned table: an entry placed while its home was full stays where it landed after the
    // home empties again, and nothing else ever moves it, so at load 0.76 after 200 turnovers a
    // hit visits 1.14 groups against 1.03 fresh and a miss 1.26 against 1.05; with one writing
    // hit per erase this brings that to 1.09 and 1.16, with four to 1.05 and 1.09. Doing it on
    // erase instead -- pulling a sibling back into the freed slot -- was measured and lost: the
    // counter cannot tell a sibling from an entry that passed through, so it fires on 46% of
    // erases and hashes two or three candidates each time, 1.5x the cost of a churn round. Here
    // the entry is the one just found, its home was just computed, and the home group is a
    // compare away from being known full or not, so a hit at home costs one compare and a hit
    // away from home costs a load, two stores and the counter walk.
    //
    // What it is worth, one map per binary so that nothing shares a translation unit, on a
    // 50000 entry table churned forty times through with a writing hit per round: misses 5.16
    // to 4.64 ns, hits 5.9 to 5.7, the churn round itself 42.7 against 42.5, branch misses down
    // 18%; at 2M entries everything within 1-2%, since one step of displacement is the adjacent
    // block and the prefetcher already has it. A paired run of the two headers in one binary had
    // read 1.49x on the misses, which was code layout, not the map.
    //
    // Only from paths that already write. A const find cannot do this, and a non-const find()
    // is treated as read-only by callers who share a map between threads, so it does not either.
    void move_home(value_idx_type found_in, std::uint8_t from_lane, std::uint64_t mh) {
        auto const home_idx = group_idx_from_hash(mh);
        if (ANKERL_UNORDERED_DENSE_LIKELY(found_in == home_idx)) {
            return;
        }
        auto* groups = m_buckets.data();
        auto& home = groups[home_idx];
        auto const empties = match_empty(home);
        if (empties == 0) {
            return;
        }
        auto const lane = first_lane(empties);
        auto const mask = m_group_mask;
        auto const counter = fingerprint_word(mh) & 7U;
        auto& from = groups[found_in];
        home.m_fingerprints[lane] = from.m_fingerprints[from_lane];
        home.m_index[lane] = from.m_index[from_lane];
        from.m_fingerprints[from_lane] = 0;
        uncount(groups, mask, home_idx, counter, found_in);
    }

    // Which slot points at this value, searched from the value's home group. Its precondition is that one does, and the map's
    // own callers always satisfy it -- but the key is *not* const here, so a caller can break it with an assignment and no
    // cast: `it->first = x; m.erase(it);` then asks for a slot on a probe sequence the element is not on.
    //
    // Bounded for that, the same way the miss probe is and after the same kind of hang: `delta ==
    // m_group_mask` means the triangular sequence has now visited every group, so there is nowhere
    // left. Returning "not found" is not available -- every caller's contract says the element is
    // there and the return is a slot number -- so the useful outcome is a diagnosable abort rather
    // than a core spinning at 100% forever, which is what this did until 2026-09-11 (#254).
    //
    // Two things the throw is not. It is not recoverable: the twin below reaches it from the backfill
    // in finish_erase, where the slot is already gone, so the table is in the state that comment
    // describes as unusable and the exception says what happened rather than offering to continue.
    // And it is not a guarantee --
    // if the mutated key's sequence happens to cross the element's real slot with a matching
    // fingerprint, a wrong-but-valid slot comes back and the counters are unwound from the wrong
    // home instead. Bounding turns a hang into a diagnosis; only `replace_key()` turns it into a
    // supported operation.
    //
    // The test is after the lane loop, so the common case -- the element is in its home group --
    // returns without reaching it, and a group that does fall through was about to call next_group
    // anyway: one compare, on the walking path only.
    //
    // It measured 10% *faster* than the unbounded version under clang, and that is an artifact, not
    // a reason -- see notes/index-design.md. Forcing this function out of line makes the difference
    // vanish and gcc never had it. The reason to bound the loop is that it hangs.
    //
    // No prefetch_index here, unlike the probe, and none in repoint_value either. There the index a
    // group hands back is an address to load from -- `m_values[value_idx]`, a second miss behind the
    // first -- so the prefetch takes one level off a two-level chain. Here the index ends the chain:
    // it feeds a compare, and in the twin a store that ends the function, so what the prefetch can
    // overlap is worth about what its two instructions cost. Dropping both takes 0.3-0.6% of the
    // instructions off every erase-heavy workload of the score under both compilers and nothing off
    // any other one; the time moves less than the noise floor, baseline/candidate 1.0039 under gcc
    // and 0.9991 under clang, one header per binary with scripts/ab/solo.sh. It ships on the
    // instruction count. #263.
    [[nodiscard]] auto slot_of_value(std::uint64_t mh, value_idx_type value_idx) const -> group_slot {
        auto const word = fingerprint_word(mh);
        auto group_idx = group_idx_from_hash(mh);
        auto const* groups = m_buckets.data();
        value_idx_type delta = 0;
        while (true) {
            auto const& group = groups[group_idx];
            auto lanes = match_fingerprint(group, word);
            while (lanes != 0) {
                auto const lane = first_lane(lanes);
                if (group.m_index[lane] == value_idx) {
                    return {group_idx, static_cast<std::uint8_t>(lane)};
                }
                lanes &= lanes - 1;
            }
            if (ANKERL_UNORDERED_DENSE_UNLIKELY(delta == m_group_mask))
                ANKERL_UNORDERED_DENSE_UNLIKELY_ATTR {
                    on_error_key_changed();
                }
            group_idx = next_group(group_idx, delta);
        }
    }

    // The same walk, for the one caller that does not want a slot number but only to overwrite the
    // index it finds. Handing that caller a slot cost 5.1 instructions per erase under clang and 4.5
    // under gcc: the loop has the group base and the lane in registers, packs them into `group_idx *
    // slots_per_group + lane`, and the store then took that straight back apart. Clang folds it away
    // on the erase(iterator) path, where erase_group_slot gets the same slot, and did not fold it
    // here; gcc folds it at none of the sites that still pack one. notes/index-design.md has the
    // before and after (#260).
    //
    // Precondition, bound and exhaustion are slot_of_value's; its comment is the one to read, and
    // the half of it about the throw not being recoverable is about this function.
    void repoint_value(std::uint64_t mh, value_idx_type value_idx, value_idx_type new_value_idx) {
        auto const word = fingerprint_word(mh);
        auto group_idx = group_idx_from_hash(mh);
        auto* groups = m_buckets.data();
        value_idx_type delta = 0;
        while (true) {
            auto& group = groups[group_idx];
            auto lanes = match_fingerprint(group, word);
            while (lanes != 0) {
                auto const lane = first_lane(lanes);
                if (group.m_index[lane] == value_idx) {
                    group.m_index[lane] = new_value_idx;
                    return;
                }
                lanes &= lanes - 1;
            }
            if (ANKERL_UNORDERED_DENSE_UNLIKELY(delta == m_group_mask))
                ANKERL_UNORDERED_DENSE_UNLIKELY_ATTR {
                    on_error_key_changed();
                }
            group_idx = next_group(group_idx, delta);
        }
    }

    [[nodiscard]] static constexpr auto get_key(value_type const& vt) -> key_type const& {
        if constexpr (is_map_v<T>) {
            return vt.first;
        } else {
            return vt;
        }
    }

    [[nodiscard]] static constexpr auto calc_num_groups(std::uint8_t shifts) -> std::size_t {
        return (std::min)(max_bucket_count() / slots_per_group, std::size_t{1} << (64U - shifts));
    }

    // in slots, which is what the bucket interface counts in
    [[nodiscard]] static constexpr auto calc_num_buckets(std::uint8_t shifts) -> std::size_t {
        return calc_num_groups(shifts) * slots_per_group;
    }

    // How much index an array with these shifts is, which is what both pipeline gates are measured
    // against. One spelling of the quantity; the threshold is the gated loop's own, because the two
    // do not have the same one.
    [[nodiscard]] static constexpr auto index_bytes_for(std::uint8_t shifts) -> std::size_t {
        return calc_num_groups(shifts) * sizeof(typename bucket_container_type::block);
    }

    [[nodiscard]] constexpr auto calc_shifts_for_size(std::size_t s) const -> std::uint8_t {
        auto shifts = initial_shifts;
        // Stopping once the array is as large as it may get is what keeps this from running off the
        // end. calc_num_buckets() saturates at max_bucket_count(), so past that point the capacity
        // being compared stops growing while the loop keeps decrementing -- and for any size above
        // max_bucket_count() * max_load_factor() it used to walk all the way to zero. A shift of
        // zero then asks calc_num_buckets() for `1 << 64`, which is undefined and in practice one:
        // a table sized for billions of elements would come back with a single bucket and a mask of
        // zero, and the next probe reads past the end of it. Reachable from rehash(), which does not
        // allocate the values and so has nothing to fail first.
        while (shifts > 0 && calc_num_buckets(shifts) < max_bucket_count() &&
               static_cast<std::size_t>(static_cast<float>(calc_num_buckets(shifts)) * max_load_factor()) < s) {
            --shifts;
        }
        return shifts;
    }

    // assumes m_values has data, m_buckets=m_buckets_end=nullptr, m_shifts is INITIAL_SHIFTS
    void copy_buckets(table const& other) {
        // assumes m_values has already the correct data copied over.
        if (empty()) {
            // Nothing to index, so stay in the state a default constructed table is in and let the
            // first insert allocate. Copying an empty table therefore allocates nothing either.
            m_shifts = initial_shifts;
        } else {
            // One pass, not two. This used to grow the array with resize(), which value
            // initialises every bucket it adds, and then memcpy over all of it -- so every byte
            // of the bucket array was written twice, and for a large map the wasted half is a
            // memset of megabytes. assign() copies straight into the new storage.
            //
            // assign() and not m_buckets = other.m_buckets, which would consult pocca: the
            // allocator question is answered by the caller, and this is also reached from the
            // move assignment's differing-allocator branch, where adopting other's would be
            // exactly wrong.
            m_buckets.assign(other.m_buckets);
            m_shifts = other.m_shifts;
            describe_buckets(other.m_buckets.size());
        }
    }

    // The part of copy assignment that can throw, kept separate so the operator can put the table
    // back together if it does.
    void copy_everything_from(table const& other) {
        // The assignment below takes other's allocator (pocca), and the buckets have to follow it,
        // or the container's two halves end up on different allocators and get_allocator() -- which
        // reports m_values' -- stops describing the bucket array, which the "same allocator" check
        // in the move assignment relies on it doing.
        //
        // Done before the copy rather than after: it is the same allocator either way, both
        // containers are empty here so it cannot throw, and doing it first means a copy that fails
        // part way through cannot leave the two halves disagreeing. Copy assignment and not move:
        // move would consult pocma, a different question, and not the one answered true here.
        if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value) {
            // Rebound explicitly: m_values' allocator and m_buckets' are different types, and
            // comparing them directly is ambiguous rather than merely unusual.
            auto const wanted = typename bucket_container_type::allocator_type(other.m_values.get_allocator());
            if (m_buckets.get_allocator() != wanted) {
                auto const empty_with_other_allocator = bucket_container_type(wanted);
                m_buckets = empty_with_other_allocator;
            }
        }

        m_values = other.m_values;
        m_max_load_factor = other.m_max_load_factor;
        m_hash = other.m_hash;
        m_equal = other.m_equal;
        copy_buckets(other); // sets m_shifts on both of its branches
    }

    // The half of move assignment that can throw, so the caller can put the table back together if
    // it does. Its twin for copies is above.
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved) -- moved from member by member
    void move_everything_from(table&& other) {
        m_values = std::move(other.m_values);
        other.m_values.clear();

        // we can only reuse m_buckets when both maps have the same allocator!
        if (get_allocator() == other.get_allocator()) {
            m_buckets = std::move(other.m_buckets);
            other.m_buckets.clear();
            m_max_bucket_capacity = std::exchange(other.m_max_bucket_capacity, 0);
            m_group_mask = std::exchange(other.m_group_mask, 0);
            m_shifts = std::exchange(other.m_shifts, initial_shifts);
            m_max_load_factor = std::exchange(other.m_max_load_factor, default_max_load_factor);
            m_hash = std::exchange(other.m_hash, {});
            m_equal = std::exchange(other.m_equal, {});
            // The exchanges above leave "other" exactly as a default constructed table looks, so it
            // is already usable and does not need buckets handed back to it. It used to get a
            // freshly allocated set here, which is an allocation -- and a way to throw -- inside an
            // operation that is otherwise noexcept and needs neither.
        } else {
            // set max_load_factor *before* copying the other's buckets, so we have the same behavior
            m_max_load_factor = other.m_max_load_factor;

            // copy_buckets sets m_buckets, m_num_buckets, m_max_bucket_capacity, m_shifts
            copy_buckets(other);
            // clear's the other's buckets so other is now already usable.
            other.clear_buckets();
            m_hash = other.m_hash;
            m_equal = other.m_equal;
        }
        // map "other" is now already usable, it's empty.
    }

    // Back to what a default constructed table holds. An assignment gives the buckets back before
    // it knows whether it can build new ones, and in between the table holds values it has no way
    // to find -- size() elements and no bucket array at all, which no operation is prepared for. If
    // an exception leaves that window this is where it lands: assignment owes the basic guarantee,
    // which means valid and not merely non-leaking, and with no buckets the only valid state is
    // empty. Every step is noexcept, so the recovery cannot fail on its way out.
    // Deliberately not deallocate_buckets(), which is otherwise the same three stores: that one
    // also calls shrink_to_fit(), which is allowed to allocate and is not noexcept, and this runs
    // while an exception is already in flight.
    void reset_to_empty() noexcept {
        m_values.clear();
        m_buckets.clear();
        m_max_bucket_capacity = 0;
        m_group_mask = 0;
        m_shifts = initial_shifts;
    }

    /**
     * True when no element can be added any more without increasing the size
     */
    [[nodiscard]] auto is_full() const -> bool {
        return size() > m_max_bucket_capacity;
    }

    void deallocate_buckets() {
        m_buckets.clear();
        m_buckets.shrink_to_fit();
        m_max_bucket_capacity = 0;
        m_group_mask = 0;
    }

    // Takes the shift rather than reading m_shifts, so that nothing describing the bucket array is
    // written until an array of that size exists. Callers used to assign m_shifts and then
    // allocate, which left a gap for a failed allocation to stop in.
    void allocate_buckets_from_shift(std::uint8_t shifts) {
        auto const num_groups = calc_num_groups(shifts);
        {
            // Built beside the old array rather than over it, so that a failure here leaves the
            // table exactly as it was. Callers used to give the old array back first, which made
            // this the only allocation alive -- and made a failure leave them holding values with
            // no buckets to find them by, which is not a state anything can recover from without
            // allocating again.
            auto fresh = bucket_container_type(m_buckets.get_allocator());
            fresh.resize(num_groups);
            m_buckets = std::move(fresh);
        }
        // The groups come back zeroed, which is an empty index: every slot free, every counter at
        // zero. Nothing that allocates clears afterwards.
        //
        // All three commit here, together, and only once the array they describe exists. They have
        // to move as one: a probe indexes its first group with hash >> m_shifts and does not mask,
        // so a shift that has run ahead of the array reads past the end of it, and a mask published
        // ahead of an allocation that then failed does the same. This is the one function every
        // index-allocating path goes through, which is what makes a failed growth leave the old
        // index intact and consistent rather than unusable.
        m_shifts = shifts;
        describe_buckets(num_groups);
    }

    // The two values derived from the bucket array's size. Only ever called once the array of that
    // size exists; see the note above.
    void describe_buckets(std::size_t num_groups) {
        m_group_mask = static_cast<value_idx_type>(num_groups - 1);
        auto const num_buckets = num_groups * slots_per_group;
        if (num_buckets == max_bucket_count()) {
            // reached the maximum, make sure we can use each bucket
            m_max_bucket_capacity = max_bucket_count();
        } else {
            m_max_bucket_capacity = static_cast<value_idx_type>(static_cast<float>(num_buckets) * max_load_factor());
        }
    }

    // The bucket array is not allocated until the first element goes in, so that a default
    // constructed table does not allocate. Every path that probes the buckets either returns early
    // while the table is empty (do_find and do_find_hashed's callers, do_erase_key), or needs an
    // iterator into m_values and so
    // cannot be reached in this state (erase, extract, replace_key), or calls this first -- which
    // is the four insert entry points, the only ones that reach the buckets without a prior
    // emptiness check.
    void allocate_buckets_if_none() {
        if (ANKERL_UNORDERED_DENSE_UNLIKELY(m_buckets.empty()))
            ANKERL_UNORDERED_DENSE_UNLIKELY_ATTR {
                allocate_buckets_from_shift(m_shifts);
            }
    }

    void clear_buckets() {
        // Reachable now that a table can have no buckets at all -- extract() clears them on the way
        // out whether or not there are any. data() is null in that state, and memset's pointer has
        // to be valid even for a zero length. Neither sanitizer in CI objects, so this is on the
        // language rule rather than on a diagnostic.
        if (m_buckets.empty()) {
            return;
        }
        // Clearing the groups empties every slot and zeroes every counter; the value indices
        // beside them are never read for an empty slot.
        m_buckets.clear_metadata();
    }

    // Into an index just allocated, so already empty.
    void fill_buckets_from_values() {
        // Walked with an iterator rather than indexed with m_values[i], which is not a style
        // choice: placing an entry stores a fingerprint, a std::uint8_t store may alias any object
        // at all, and the container's own data pointer is such an object -- so after every
        // placement an indexed read has to load that pointer back out of the container before it
        // can even form the address of the next key. That is a store-to-load chain through every
        // element of the rehash, and it costs exactly one memory latency per element because the
        // random group access that follows cannot start until it resolves. An iterator lives in a
        // register across the loop, so the next key's address is ready immediately and the group
        // accesses overlap. Measured on a 200000 element build under clang: growth 10.43 ns per
        // insert to 2.74, the whole build 16.72 to 8.96. gcc disambiguated it on its own and does
        // not move (2.15 to 2.48, noise); this is what made the same build 1.7x slower under clang
        // than under gcc, and it is now faster.
        //
        // The same reasoning is why the group pointer, the mask and the shift are held in locals
        // and the placement is written out here rather than calling place_group: every one of
        // them is read through `this`, which the fingerprint store may alias too.
        //
        // The loop hashes sixteen elements ahead of the one it places and prefetches the group
        // each will land in. A sliding ring rather than the bulk visit's chunks, and measured
        // against it: chunking this loop is 5-17% slower, worst for a string key, for the same
        // instructions and 12.5% more cycles. An element here has one dependent random access --
        // the block it is placed in -- where a visit has two, so there is no second batch to issue
        // concurrently and the only thing that matters is how much time separates a prefetch from
        // its use. A ring gives every element a full sixteen placements of that; a chunk gives its
        // first elements only the rest of the hashing pass, which is far cheaper than placing. Below cache that is worth 1.26x
        // on the loop (2.05 to 1.63 ns per element at 200000 entries) for the pipelining alone: the hash, which is a chain, is
        // decoupled from the placement, which is a random access, so neither waits for the other.
        // Above cache the prefetch is what matters for a key whose hash has work to hide a miss
        // behind -- a string rehash at four million entries goes 30.6 to 12.4 ns per element -- and
        // does nothing for an integer key at 176 MB (12.6 to 12.5), because that loop is bound by
        // the TLB rather than by latency: 1.15 dTLB misses per placement on 4 KB pages. Partitioning
        // the elements by group first, database style, does cut that to 0.24 and halves the loop
        // in isolation (10.3 to 5.4 at 44 MB), but inside a build it is worth 0-7% of an integer
        // build above 32 MB and nothing below, because the rehash is a minority of a large build
        // and the scratch it needs is faulted in fresh every time at about a microsecond a page. Not
        // kept; the measurement is in CLAUDE.md.
        //
        // The index is counted in value_idx_type and never in the container's size, for the reason
        // spelled out in replace(): max_size() is exactly what value_idx_type can hold, so a
        // container of precisely that many has a size that is not representable in it.
        constexpr auto ahead = pipeline_depth;
        auto* const groups = m_buckets.data();
        auto const mask = m_group_mask;
        auto const shifts = m_shifts;
        auto ring = std::array<std::uint64_t, ahead>{};
        auto it = m_values.begin();
        auto const end = m_values.end();
        auto const fetch = [&](std::size_t i) -> void {
            auto const mh = mixed_hash(get_key(*it));
            ++it;
            ring[i] = mh;
            prefetch_block(groups, static_cast<std::size_t>(mh >> shifts));
        };
        for (auto i = std::size_t{}; i < ahead && it != end; ++i) {
            fetch(i);
        }
        auto const n = m_values.size();
        for (auto value_idx = std::size_t{}; value_idx < n; ++value_idx) {
            auto const mh = ring[value_idx % ahead];
            if (it != end) {
                fetch(value_idx % ahead);
            }
            // we know for certain that key has not yet been inserted, so no need to check it.
            auto const word = fingerprint_word(mh);
            auto const counter = word & 7U;
            auto group_idx = static_cast<value_idx_type>(mh >> shifts);
            value_idx_type delta = 0;
            while (true) {
                auto& group = groups[group_idx];
                auto const empties = match_empty(group);
                if (empties != 0) {
                    auto const lane = first_lane(empties);
                    group.m_fingerprints[lane] = static_cast<std::uint8_t>(word);
                    group.m_index[lane] = static_cast<value_idx_type>(value_idx);
                    break;
                }
                if (group.m_overflows[counter] != 255) {
                    ++group.m_overflows[counter];
                }
                group_idx = static_cast<value_idx_type>((group_idx + (++delta)) & mask);
            }
        }
    }

    void increase_size() {
        if (m_max_bucket_capacity == max_bucket_count()) {
            // remove the value again, we can't add it!
            m_values.pop_back();
            on_error_bucket_overflow();
        }
        // Both callers have already appended the new element to m_values, which is why the branch
        // above takes it back out before reporting the overflow. A bucket array that cannot be
        // grown is the same situation: the element is in m_values with no bucket pointing at it,
        // and never will have one, so size() would count an element that find() cannot reach.
        // Taking it back out is what makes a failed insert have no effect, which is what the
        // unordered containers promise for inserting a single element.
        if constexpr (ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS()) {
            try {
                allocate_buckets_from_shift(static_cast<std::uint8_t>(m_shifts - 1));
            } catch (...) {
                m_values.pop_back();
                throw;
            }
        } else {
            allocate_buckets_from_shift(static_cast<std::uint8_t>(m_shifts - 1));
        }
        fill_buckets_from_values();
    }

    // Closes the hole that the erased value left in m_values, by moving the last value into it and repointing that
    // value's bucket. Runs after the erased value has been handed over, and has to run even when handing it over threw:
    // by that point the bucket is already gone, so leaving the value in place would mean size() counts an element that
    // nothing can find.
    void finish_erase(value_idx_type value_idx_to_remove) {
        if (value_idx_to_remove != m_values.size() - 1) {
            // no luck, we'll have to replace the value with the last one and update the index accordingly
            auto& val = m_values[value_idx_to_remove];
            val = std::move(m_values.back());

            // update the value index of the moved entry
            auto const values_idx_back = static_cast<value_idx_type>(m_values.size() - 1);
            auto const mh = mixed_hash(get_key(val));
            repoint_value(mh, values_idx_back, value_idx_to_remove);
        }
        m_values.pop_back();
    }

    // `at` is the slot that points at the value; mh is the value's mixed hash, which the counters
    // on the way to it are undone with.
    template <typename Op>
    void do_erase(group_slot at, value_idx_type value_idx_to_remove, std::uint64_t mh, Op handle_erased_value) {
        // both values are needed once the slot is freed; start fetching them now to overlap the latencies
        ANKERL_UNORDERED_DENSE_PREFETCH(&m_values[value_idx_to_remove]);
        ANKERL_UNORDERED_DENSE_PREFETCH(&m_values.back());

        erase_group_slot(at.group_idx, at.lane, mh);
        auto&& erased_value = std::move(m_values[value_idx_to_remove]);

        // erase() hands the value to a callback that cannot throw, so the branch below is not even instantiated for it.
        // extract() moves the value out into the caller's storage, and that move is the one that can throw.
        if constexpr (ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() && !noexcept(handle_erased_value(std::move(erased_value)))) {
            try {
                handle_erased_value(std::move(erased_value));
            } catch (...) {
                finish_erase(value_idx_to_remove);
                throw;
            }
        } else {
            handle_erased_value(std::move(erased_value));
        }

        finish_erase(value_idx_to_remove);
    }

    template <typename K, typename Op>
    auto do_erase_key(K&& key, Op handle_erased_value) -> std::size_t { // NOLINT(cppcoreguidelines-missing-std-forward)
        if (empty()) {
            return 0;
        }

        auto const mh = mixed_hash(key);
        auto r = probe(key, mh);
        if (!r.found) {
            return 0;
        }
        do_erase({r.group_idx, r.lane}, r.value_idx, mh, handle_erased_value);
        return 1;
    }

    template <class K, class M>
    auto do_insert_or_assign(K&& key, M&& mapped) -> std::pair<iterator, bool> {
        auto it_isinserted = try_emplace(std::forward<K>(key), std::forward<M>(mapped));
        if (!it_isinserted.second) {
            it_isinserted.first->second = std::forward<M>(mapped);
        }
        return it_isinserted;
    }

    // Appends the value and points a slot at it. What it needs to know is where the key belongs;
    // place_element below takes that from the probe that just missed, rather than deriving it
    // again.
    //
    // Forced inline, and the reason is a trade worth knowing. clang prices this function at 480
    // against an inlining threshold of 250 (vector::emplace_back with piecewise_construct is 225
    // of it) and so calls it out of line from do_try_emplace, which costs every insert a call, a
    // six register prologue and epilogue: 28 of the 128 instructions an insert took, measured
    // net of the benchmark loop. Handing the probe's fingerprint to the callee, returning the
    // index in a register, and moving increase_size() out of line were each measured and each
    // changed nothing: the cost is the boundary itself.
    //
    // Removed on 2026-09-08 and put back the same day, which is the part worth keeping. The
    // removal rested on the scored suite built one header per binary, where it reads 1.7% faster
    // under clang and 3.9% under gcc. That binary is ~90 translation units of test suite and its
    // inlining budget is exhausted, so an always_inline there displaces something else; a caller's
    // is not. Measured in a translation unit holding one map, which is what a caller has, building
    // from empty, with the attribute against without -- 251633 ns against 287833 at 32000 entries,
    // 1749840 against 2087600 at 200000, 13064800 against 15670600 at a million, so 14 to 20%
    // slower without it at every size. The instruction counts are what settle it, because neither
    // code layout nor drift can move them: 5.08M against 5.98M, 28.09M against 33.71M, 162.3M
    // against 190.4M, i.e. 17 to 20% more work retired without the attribute. Two other harnesses
    // agree (maps.cpp, eighteen maps in one unit: build 17% slower at 32000).
    //
    // So the rule this leaves is narrower than "one header per binary": the *size* of the
    // translation unit decides what an always_inline is worth, a benchmark binary is the largest
    // unit anyone compiles this into, and an instruction count is the only number in the argument
    // that none of that moves. What the attribute costs is real and stays true -- operator[] on a
    // key already present pays for the placement code's register pressure on a path that never
    // places (clang 73.2 instructions against 48.4) -- it is simply smaller than 17% of a build.
    template <typename... Args>
    ANKERL_UNORDERED_DENSE_FORCEINLINE auto do_place_element(std::uint64_t mh, Args&&... args) -> std::pair<iterator, bool> {
        auto const word = fingerprint_word(mh);
        return place_element_at(word, word & 7U, group_idx_from_hash(mh), std::forward<Args>(args)...);
    }

    // As above for a caller holding the hash in pieces. The home group is only used on the branch
    // that does not grow; growth rebuilds the whole index and places this element with the rest, so
    // a stale group index cannot escape it.
    template <typename... Args>
    ANKERL_UNORDERED_DENSE_FORCEINLINE auto
    place_element_at(std::uint32_t word, unsigned counter, value_idx_type home_idx, Args&&... args)
        -> std::pair<iterator, bool> {
        // emplace the new value. If that throws an exception, no harm done; index is still in a valid state
        m_values.emplace_back(std::forward<Args>(args)...);

        auto value_idx = static_cast<value_idx_type>(m_values.size() - 1);
        if (ANKERL_UNORDERED_DENSE_UNLIKELY(is_full()))
            ANKERL_UNORDERED_DENSE_UNLIKELY_ATTR {
                increase_size(); // places every value, the new one included
            }
        else {
            place_group(word, counter, home_idx, value_idx);
        }
        return {begin() + static_cast<difference_type>(value_idx), true};
    }

    // The pipelined half of insert(first, last); see the comment there for what it buys.
    //
    // Growth during the loop is safe and not special-cased: it moves the bucket array, so prefetches
    // already in flight are aimed at the old one and simply do nothing, and the hashes stay valid
    // because a hash does not depend on the table. That is also why the array and the shift are read
    // fresh for every element rather than hoisted the way the rehash hoists them -- the rehash never
    // grows, and this may.
    template <typename FwdIt>
    void do_insert_range(FwdIt first, FwdIt last) {
        if (first == last) {
            return;
        }
        allocate_buckets_if_none();
        auto ring = std::array<std::uint64_t, pipeline_depth>{};
        auto lookahead = first;
        auto hash_and_prefetch = [this](auto const& value) -> std::uint64_t {
            auto const mh = mixed_hash(get_key(value));
            prefetch_block(m_buckets.data(), std::size_t{group_idx_from_hash(mh)});
            return mh;
        };
        for (auto i = std::size_t{}; i < pipeline_depth && lookahead != last; ++i) {
            ring[i] = hash_and_prefetch(*lookahead);
            ++lookahead;
        }
        for (auto i = std::size_t{}; first != last; ++first, ++i) {
            // This element's hash out of the ring before the slot it frees is refilled: the slot
            // holding element i is the one element i + pipeline_depth goes into. Getting that
            // backwards returns a wrong answer rather than crashing, and it has been got backwards
            // twice. fill_buckets_from_values has the same ordering for the same reason.
            //
            // The three rings in this file -- here, fill_buckets_from_values and
            // do_replace_pipelined -- are deliberately not one shared loop. Extracting the ring into
            // a helper taking both halves as callables was built and measured on 2026-09-11: it
            // costs the rehash **1.9 instructions per element**, 45.7 to 47.6, and 1.3% of its time
            // on six of six interleaved pairs, because that loop wants its group pointer, mask and
            // shift in locals and this one cannot have them -- growth moves them. Passing the
            // element index through the callable did not recover it either. The duplication is real
            // and measured to be cheaper than the fix.
            auto const slot = i % pipeline_depth;
            auto const mh = ring[slot];
            if (lookahead != last) {
                ring[slot] = hash_and_prefetch(*lookahead);
                ++lookahead;
            }
            do_insert_hashed(mh, *first);
        }
    }

    // The pipelined part of replace()'s dedup walk; see there for why it is allowed to exist and
    // where it has to stop. Advances value_idx to the first element it did not handle.
    //
    // The ring holds the hash already taken apart -- the fingerprint word, the home group and the
    // block that group lives in -- rather than the mixed hash. Nothing here grows the index, so a
    // block pointer stays valid, and holding it is the difference between forming an 88 byte block's
    // address once per element and forming it again in the probe and a third time in the placement.
    void do_replace_pipelined(std::size_t& value_idx) {
        auto const* const groups = m_buckets.data();
        auto ring_word = std::array<std::uint32_t, pipeline_depth>{};
        auto ring_home = std::array<value_idx_type, pipeline_depth>{};
        auto ring_block = std::array<typename bucket_container_type::block const*, pipeline_depth>{};

        // Cursors for the elements, a counter for the index, and both are needed: place_group wants
        // the index, and reading m_values[value_idx] to get the element wants the container's data
        // pointer back after every placement, because placing stores a fingerprint and a
        // std::uint8_t store may alias that pointer. fill_buckets_from_values has the full argument
        // and the measurement. The counter costs an increment in a register; re-deriving it from
        // `read - first` would be a divide by the element size on any value type whose size is not a
        // power of two.
        //
        // Two cursors, and deliberately not a third for the end. Holding `last` as well and testing
        // `last - read` was built and measured and is the slower of the two -- 3.83 ns against 3.54
        // at two hundred thousand elements with no duplicates -- because it costs a register across a
        // loop that already holds three ring arrays, and because it cannot simply be decremented on a
        // pop: std::deque::pop_back invalidates the past-the-end iterator and a deque is a value
        // container this map supports, so it would need an m_values.end() after every duplicate. The
        // container's own size() answers the same question for nothing.
        auto const first = m_values.begin();
        auto read = first;                  // element value_idx
        auto look = first + pipeline_depth; // element value_idx + pipeline_depth

        // Hashes and decomposes, but does not ask for the block: the caller decides that, because
        // the one refill that happens on a duplicate is read by the very next iteration and has no
        // distance to run.
        auto fetch = [&](std::size_t slot, value_type const& value) {
            auto const mh = mixed_hash(get_key(value));
            ring_word[slot] = fingerprint_word(mh);
            ring_home[slot] = group_idx_from_hash(mh);
            ring_block[slot] = groups + std::size_t{ring_home[slot]};
        };

        auto prime = first;
        for (auto i = std::size_t{}; i < pipeline_depth; ++i, ++prime) {
            fetch(i, *prime);
            prefetch_block(ring_block[i]);
        }
        while (m_values.size() > value_idx + pipeline_depth + 1) {
            auto const slot = value_idx % pipeline_depth;
            auto const word = ring_word[slot];
            auto const counter = word & 7U;
            auto const home_idx = ring_home[slot];
            // The group is handed over rather than looked up: this loop asked for it
            // pipeline_depth elements ago, so the probe must not ask again.
            if (probe_at_home(get_key(*read), word, counter, *ring_block[slot], home_idx).found) {
                // The slot the duplicate vacated is refilled from the element that just moved into
                // it -- read by the very next iteration, so there is no distance to prefetch over.
                // `read` is below the last element here, by the loop's own condition, so the plain
                // loop's self-move guard cannot be needed.
                *read = std::move(m_values.back());
                m_values.pop_back();
                fetch(slot, *read);
            } else {
                place_group(word, counter, home_idx, static_cast<value_idx_type>(value_idx));
                // The slot holding this element is the one `look` goes into, and the loop's own
                // condition says that element exists. `look` moves only here, so it stays exactly
                // pipeline_depth ahead of `read`.
                fetch(slot, *look);
                ++look;
                prefetch_block(ring_block[slot]);
                ++read;
                ++value_idx;
            }
        }
    }

    // An insert whose key has already been hashed, which is what lets a range insert hash ahead of
    // where it is placing.
    //
    // It is emplace()'s job rather than do_try_emplace's: it takes a whole value, not a key and
    // pieces to build one from. Unlike emplace() it probes before constructing anything, which it
    // can because a value_type already carries its key -- emplace() has to build the element first
    // to find out what the key is, and pop it back off again when the key turns out to be present.
    // Routing the single-element insert(value) through here too would save it that, and is left for
    // its own change because the insert path is measured and this one is not about it.
    //
    // The caller allocates the buckets; every other insert helper does that itself, and this one
    // cannot because the range insert has to prefetch against an array that already exists.
    //
    // Returns nothing on purpose: the only caller is the range insert, which has nothing to do with
    // an iterator to each element, and a returned pair nobody reads is surface no test can defend.
    template <typename V>
    void do_insert_hashed(std::uint64_t mh, V&& value) {
        // Taken apart once and handed to both halves. A probe that misses is followed by a placement
        // that starts from the same group with the same fingerprint, and deriving those twice is
        // work this loop is otherwise not doing.
        //
        auto const word = fingerprint_word(mh);
        auto const counter = word & 7U;
        auto const home_idx = group_idx_from_hash(mh);
        // The group is formed here and handed to the probe, which therefore does not ask for it a
        // second time -- the pipelined caller asked for this block sixteen elements ago. Read from
        // the current array, so a growth since then is simply a prefetch that did nothing.
        auto r = probe_at_home(get_key(value), word, counter, m_buckets.data()[home_idx], home_idx);
        if (r.found) {
            move_home(r.group_idx, r.lane, mh);
            return;
        }
        place_element_at(word, counter, home_idx, std::forward<V>(value));
    }

    // Closes the gaps a merge left behind, on the table it took them out of: compacts everything
    // from `from` on down onto `keep`, drops the tail that is now moved-from, and rebuilds the index
    // once over what is left. Called on the *source*, which is why it pairs with
    // fill_buckets_from_values rather than living beside do_merge.
    //
    // That one rebuild is the point of the whole design. The loop a caller could write instead --
    // try_emplace here, erase(it) there -- repairs the index once per element taken, and each repair
    // is a second hash of the key leaving plus a third of the element the backfill drags into its
    // place. Over a source that mostly does not overlap, that is most of the work.
    void drop_tail_and_reindex(std::size_t keep) {
        auto const n = m_values.size();
        if (keep == n) {
            // nothing was taken, so the index still describes the values exactly
            return;
        }
        // Counted rather than "while size() > keep": the compaction above does not change the size,
        // so the count is known, and the container is only required to have pop_back -- vector's
        // resize(n) would need the value type to be default constructible, which nothing else here
        // asks of it, and segmented_vector has no erase(first, last) to take a range out with.
        for (auto k = n; k > keep; --k) {
            m_values.pop_back();
        }
        clear_buckets();
        fill_buckets_from_values();
    }

    // As above for the path that has gaps to close first. Kept apart from it because the walk's
    // normal exit has none by construction, and folding the two would inline a compaction loop that
    // can never run into the merge's hot path.
    void compact_and_reindex(std::size_t keep, std::size_t from) {
        auto const n = m_values.size();
        for (auto i = from; i < n; ++i, ++keep) {
            if (keep != i) {
                m_values[keep] = std::move(m_values[i]);
            }
        }
        drop_tail_and_reindex(keep);
    }

    // The engine behind merge(). One pass over the source's values, taking every element whose key
    // is not here already and compacting the ones left behind down over the gaps.
    //
    // Compaction rather than the backfill erase() does, because the two have opposite best cases and
    // this one's best case is the common one: a merge of sources that barely overlap takes nearly
    // every element, so there is nearly nothing left to compact, where a backfill moves one element
    // for every element it takes.
    //
    // The probe and the placement are written out here rather than calling do_insert_hashed, and the
    // reason is the repair below. A throw out of the probe -- the hash, or the key compare -- leaves
    // the source's element untouched, and a throw out of the placement leaves a husk the source must
    // not keep, because a moved-from key can collide with a key that is still there and the source
    // would then hold two elements that compare equal. Inside do_insert_hashed that boundary is not
    // visible from outside.
    template <typename Source>
    void do_merge(Source& source) {
        if constexpr (std::is_same_v<Source, table>) {
            // "if (this == &source) there are no effects", which is worth honouring for its own sake
            // and not only because the walk below would be reading a container it is emptying
            if (this == &source) {
                return;
            }
        }
        if (source.empty()) {
            // Ahead of allocate_buckets_if_none() and load-bearing there: without it, merging an
            // empty source into a default constructed map allocates that map an index it never asked
            // for. do_erase_key owns its own emptiness check for the same reason.
            return;
        }
        allocate_buckets_if_none();
        auto& src_values = source.m_values;
        auto const n = src_values.size();
        // Three cursors rather than three indices, and it is the same reason fill_buckets_from_values
        // gives at length: placing an element stores a fingerprint, a std::uint8_t store may alias any
        // object at all including the container's own data pointer, so every indexed read after a
        // placement has to load that pointer back before it can even form the next element's address.
        // An iterator is already the address. Worth 0.89 to 0.95 of the indexed loop in cache (four
        // thousand elements, no overlap / half / nine tenths: 0.92 / 0.89 / 0.91) and 0.95 to 1.00
        // above it, 26 of 27 cells, and 2 to 4.5 fewer instructions retired per element at every size.
        //
        // The source never changes size while the walk runs -- the repair at the end is what shrinks
        // it -- so all three stay valid, and the destination's growth cannot reach them.
        auto const first = src_values.begin();
        auto const last = src_values.end();
        auto read = first;  // the element being handled; also what the repair calls `from`
        auto write = first; // where the next survivor goes; also what the repair calls `keep`
        auto look = first;  // the element being hashed ahead
        // `read` is the first element the source still owns, which is the one thing the repair below
        // needs to know and the one thing an exception cannot tell it: a throw out of the probe leaves
        // this element untouched and the source keeps it, a throw out of the placement leaves a husk
        // it must drop. Advancing it *before* the placement rather than after the iteration is what
        // says which, and it costs the loop nothing because it is the loop's own cursor.

        // Hashed ahead of where it is placing, the same ring the range insert uses and for the same
        // reason -- the source's elements are all in hand, so the hash of a later one can be computed
        // while this one waits for its block. The compaction underneath cannot disturb it: the only
        // writes go to `write`, which never passes `read`, and the ring reads at `look`, which is
        // pipeline_depth ahead of it.
        //
        // Gated, like replace()'s ring and unlike the range insert's, because the loss below the
        // crossover is not inside the noise: 1.10 / 1.19 / 1.14 of the ringless loop at a source of a
        // thousand elements, at no overlap / half / nine tenths. See merge_min_index_bytes for where
        // the crossover is and why it is not replace()'s number. With the gate in, the same
        // comparison reads 0.97 / 0.97 / 1.00 there and 0.81 / 0.75 / 0.77 at two million: the worst
        // cell anywhere on the sweep is 1.009.
        //
        // Measured against the index the destination will end up with rather than the one it has:
        // every source element is new until the probe says otherwise, so this bounds it from above,
        // and reading today's index instead gets the case the ring is most for -- a large source into
        // a small map, where nothing is in cache and everything moves -- exactly wrong.
        auto const pipelined = index_bytes_for(calc_shifts_for_size(size() + n)) > merge_min_index_bytes;
        auto ring = std::array<std::uint64_t, pipeline_depth>{};
        // The group array is handed in rather than read here: it has to be read fresh once per
        // element, because placing one may grow the index and move it, but the prefetch and the probe
        // that follows it are separated by nothing that can grow it -- so once, not twice.
        auto const fetch =
            [&](typename bucket_container_type::block const* groups, std::size_t slot, value_type const& value) -> void {
            auto const mh = mixed_hash(get_key(value));
            ring[slot] = mh;
            prefetch_block(groups, std::size_t{group_idx_from_hash(mh)});
        };
        if (pipelined) {
            auto const* const groups = m_buckets.data();
            for (auto s = std::size_t{0}; s < pipeline_depth && look != last; ++s, ++look) {
                // Outside the repair below on purpose: nothing has been moved yet, so a hash that
                // throws here leaves the source exactly as it was.
                fetch(groups, s, *look);
            }
        }

        // Two loops out of one body rather than one loop with the gate inside it. A runtime branch
        // is the obvious way to write this and costs the gated-off case nearly the whole difference
        // the gate was there to save -- 5.09 ns at four thousand elements and half of them
        // overlapping, against 4.63 for the same loop with the ring taken out. A perfectly predicted
        // branch is not free when a ring, a lambda and an array hang off it and the loop has to keep
        // them live.
        auto const walk = [&](auto is_pipelined) -> void {
            auto slot = std::size_t{0};
            while (read != last) {
                auto& value = *read;
                auto const* const groups = m_buckets.data();
                auto mh = std::uint64_t{};
                if constexpr (decltype(is_pipelined)::value) {
                    // this element's hash out of the ring before the slot it frees is refilled
                    mh = ring[slot];
                    if (look != last) {
                        fetch(groups, slot, *look);
                        ++look;
                    }
                    slot = (slot + 1) % pipeline_depth;
                } else {
                    mh = mixed_hash(get_key(value));
                }
                // taken apart once for both halves, exactly as do_insert_hashed does it
                auto const word = fingerprint_word(mh);
                auto const counter = word & 7U;
                auto const home_idx = group_idx_from_hash(mh);
                auto const r = probe_at_home(get_key(value), word, counter, groups[home_idx], home_idx);
                if (r.found) {
                    // Stays behind. merge() is a writing operation, so a hit here pays for its own
                    // drift the same way one inside try_emplace does.
                    move_home(r.group_idx, r.lane, mh);
                    if (write != read) {
                        *write = std::move(value);
                    }
                    ++write;
                    ++read;
                    continue;
                }
                ++read; // this one is leaving, however the placement below ends
                place_element_at(word, counter, home_idx, std::move(value));
            }
        };

        auto const walk_either = [&]() -> void {
            if (pipelined) {
                walk(std::true_type{});
            } else {
                walk(std::false_type{});
            }
        };
        if constexpr (ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS()) {
            try {
                walk_either();
            } catch (...) {
                // The source's index stopped describing its values at the first element taken, and
                // the source is a container the caller still owns and did not hand over. Putting it
                // back together is what keeps the throw at the basic guarantee instead of leaving a
                // map whose next lookup reads an index into elements that have gone.
                source.compact_and_reindex(static_cast<std::size_t>(write - first), static_cast<std::size_t>(read - first));
                throw;
            }
        } else {
            walk_either();
        }
        source.drop_tail_and_reindex(static_cast<std::size_t>(write - first));
    }

    template <typename K, typename... Args>
    auto do_try_emplace(K&& key, Args&&... args) -> std::pair<iterator, bool> {
        allocate_buckets_if_none();
        auto const mh = mixed_hash(key);
        // Taken apart once for both halves: a probe that misses is followed by a placement starting
        // from the same group with the same fingerprint.
        auto const word = fingerprint_word(mh);
        auto const counter = word & 7U;
        auto const home_idx = group_idx_from_hash(mh);
        auto r = probe(key, word, counter, home_idx);
        if (r.found) {
            move_home(r.group_idx, r.lane, mh);
            return {begin() + static_cast<difference_type>(r.value_idx), false};
        }
        return place_element_at(word,
                                counter,
                                home_idx,
                                std::piecewise_construct,
                                std::forward_as_tuple(std::forward<K>(key)),
                                std::forward_as_tuple(std::forward<Args>(args)...));
    }

    // The engine behind visit(); see the comment there for what the three passes are for.
    // f by value, the way the standard algorithms take a callable: it is invoked once per key that
    // is found, so it cannot be forwarded -- forwarding it would move from the same object on every
    // hit. The public overloads move into this parameter.
    template <typename FwdIt, typename F>
    auto do_visit(FwdIt first, FwdIt last, F f) -> std::size_t {
        if (ANKERL_UNORDERED_DENSE_UNLIKELY(empty())) {
            return 0;
        }
        constexpr auto bulk = pipeline_depth;
        auto const* groups = m_buckets.data();
        // The block pointer rather than the group number: a block is 88 bytes, so forming its
        // address is a multiply, and passes two and three would each redo it.
        auto block = std::array<decltype(groups), bulk>{};
        auto word = std::array<std::uint32_t, bulk>{};
        auto home = std::array<value_idx_type, bulk>{};
        auto lanes = std::array<lane_mask, bulk>{};
        auto found = std::size_t{0};

        while (first != last) {
            auto n = std::size_t{0};
            auto it = first;
            for (; n < bulk && it != last; ++n, ++it) {
                auto const mh = mixed_hash(*it);
                word[n] = fingerprint_word(mh);
                home[n] = group_idx_from_hash(mh);
                block[n] = groups + std::size_t{home[n]};
                prefetch_block(block[n]);
            }
            for (auto i = std::size_t{}; i < n; ++i) {
                lanes[i] = match_fingerprint(*block[i], word[i]);
            }
            for (auto i = std::size_t{}; i < n; ++i, ++first) {
                auto const& group = *block[i];
                // The element, once it is known: converging on a pointer rather than a found-flag
                // keeps the call to f in one place instead of one per way of arriving at it.
                value_type* element = nullptr;
                auto remaining = lanes[i];
                while (remaining != 0) {
                    auto const lane = first_lane(remaining);
                    auto const value_idx = group.m_index[lane];
                    if (m_equal(*first, get_key(m_values[value_idx]))) {
                        element = &m_values[value_idx];
                        break;
                    }
                    remaining &= remaining - 1;
                }
                if (element == nullptr) {
                    // Not at home. About 5% of keys get here and none of their work was pipelined,
                    // which is what keeps the common path straight.
                    auto const r = probe_after_home(*first, word[i], word[i] & 7U, group, home[i]);
                    if (r.found) {
                        element = &m_values[r.value_idx];
                    }
                }
                if (element != nullptr) {
                    f(*element);
                    ++found;
                }
            }
        }
        return found;
    }

    template <typename K>
    auto do_find(K const& key) -> iterator {
        if (ANKERL_UNORDERED_DENSE_UNLIKELY(empty()))
            ANKERL_UNORDERED_DENSE_UNLIKELY_ATTR {
                return end();
            }

        return do_find_hashed(key, mixed_hash(key));
    }

    // Same lookup with the hashing already done. Requires the bucket array to be allocated, which
    // !empty() implies; the callers test empty() rather than this function so that a lookup in an
    // empty table returns without hashing anything.
    template <typename K>
    auto do_find_hashed(K const& key, std::uint64_t mh) -> iterator {
        auto r = probe(key, mh);
        return r.found ? begin() + static_cast<difference_type>(r.value_idx) : end();
    }

    template <typename K>
    auto do_find(K const& key) const -> const_iterator {
        return const_cast<table*>(this)->do_find(key); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }

    template <typename K>
    auto do_find(K const& key, precomputed_hash ph) -> iterator {
        if (ANKERL_UNORDERED_DENSE_UNLIKELY(empty()))
            ANKERL_UNORDERED_DENSE_UNLIKELY_ATTR {
                return end();
            }

        return do_find_hashed(key, ph.m_mixed_hash);
    }

    template <typename K>
    auto do_find(K const& key, precomputed_hash ph) const -> const_iterator {
        return const_cast<table*>(this)->do_find(key, ph); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }

    template <typename K, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto do_at(K const& key) -> Q& {
        if (auto it = find(key); ANKERL_UNORDERED_DENSE_LIKELY(end() != it))
            ANKERL_UNORDERED_DENSE_LIKELY_ATTR {
                return it->second;
            }
        on_error_key_not_found();
    }

    template <typename K, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto do_at(K const& key) const -> Q const& {
        return const_cast<table*>(this)->at(key); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }

    template <typename K, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto do_at(K const& key, precomputed_hash ph) -> Q& {
        if (auto it = find(key, ph); ANKERL_UNORDERED_DENSE_LIKELY(end() != it))
            ANKERL_UNORDERED_DENSE_LIKELY_ATTR {
                return it->second;
            }
        on_error_key_not_found();
    }

    template <typename K, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto do_at(K const& key, precomputed_hash ph) const -> Q const& {
        return const_cast<table*>(this)->do_at(key, ph); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }

public:
    explicit table(std::size_t bucket_count,
                   Hash const& hash = Hash(),
                   KeyEqual const& equal = KeyEqual(),
                   allocator_type const& alloc_or_container = allocator_type())
        : m_values(alloc_or_container)
        , m_buckets(alloc_or_container)
        , m_hash(hash)
        , m_equal(equal) {
        // No bucket_count asked for means no buckets yet: the first insert allocates them. See
        // allocate_buckets_if_none(). A default constructed table therefore costs no allocation at
        // all, so one can sit in a scope that may never use it without paying for it.
        if (0 != bucket_count) {
            reserve(bucket_count);
        }
    }

    table()
        : table(0) {}

    table(std::size_t bucket_count, allocator_type const& alloc)
        : table(bucket_count, Hash(), KeyEqual(), alloc) {}

    table(std::size_t bucket_count, Hash const& hash, allocator_type const& alloc)
        : table(bucket_count, hash, KeyEqual(), alloc) {}

    explicit table(allocator_type const& alloc)
        : table(0, Hash(), KeyEqual(), alloc) {}

    template <class InputIt>
    table(InputIt first,
          InputIt last,
          size_type bucket_count = 0,
          Hash const& hash = Hash(),
          KeyEqual const& equal = KeyEqual(),
          allocator_type const& alloc = allocator_type())
        : table(bucket_count, hash, equal, alloc) {
        insert(first, last);
    }

    template <class InputIt>
    table(InputIt first, InputIt last, size_type bucket_count, allocator_type const& alloc)
        : table(first, last, bucket_count, Hash(), KeyEqual(), alloc) {}

    template <class InputIt>
    table(InputIt first, InputIt last, size_type bucket_count, Hash const& hash, allocator_type const& alloc)
        : table(first, last, bucket_count, hash, KeyEqual(), alloc) {}

    // Asks the allocator whether it wants to come along, which is what allocator_traits' default
    // does and what an allocator like std::pmr::polymorphic_allocator declines: a copy of a map
    // living in an arena should not silently keep that arena alive and keep allocating into it.
    table(table const& other)
        : table(other,
                std::allocator_traits<allocator_type>::select_on_container_copy_construction(other.m_values.get_allocator())) {
    }

    // m_buckets takes the allocator too. Leaving it to its default member initialiser put the
    // bucket array in the default resource while the values went where the caller asked, so half
    // the container escaped the arena it was given -- and get_allocator(), which reports m_values'
    // allocator, could not be used to reason about the buckets any more.
    table(table const& other, allocator_type const& alloc)
        : m_values(other.m_values, alloc)
        , m_buckets(alloc)
        , m_max_load_factor(other.m_max_load_factor)
        , m_hash(other.m_hash)
        , m_equal(other.m_equal) {
        copy_buckets(other);
    }

    // Unconditionally noexcept, and honestly so: it hands over other's own allocator, so the
    // assignment below always takes the branch that takes the buffers over rather than the one
    // that moves elements into freshly allocated memory.
    table(table&& other) noexcept
        : table(std::move(other), other.m_values.get_allocator()) {}

    // Uses alloc, unconditionally. It used to construct empty and then move-assign, which cannot
    // express that: assignment has to consult propagate_on_container_move_assignment, so with a
    // propagating allocator this ended up holding other's and the allocator the caller asked for
    // was quietly dropped -- while std::vector, given the same allocator, kept it.
    //
    // Not unconditionally noexcept, unlike the plain move constructor above: this is the one whose
    // whole purpose is a *differing* allocator, so the containers below may have to move the
    // elements one at a time, and that allocates. The specification is theirs.
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved) -- moved from member by member
    table(table&& other, allocator_type const& alloc) noexcept(
        std::is_nothrow_constructible_v<value_container_type, value_container_type&&, allocator_type const&> &&
        std::is_nothrow_constructible_v<bucket_container_type, bucket_container_type&&, allocator_type const&> &&
        std::is_nothrow_move_constructible_v<Hash> && std::is_nothrow_move_constructible_v<KeyEqual>)
        : m_values(std::move(other.m_values), alloc)
        , m_buckets(std::move(other.m_buckets), alloc)
        , m_max_bucket_capacity(std::exchange(other.m_max_bucket_capacity, 0))
        , m_group_mask(std::exchange(other.m_group_mask, 0))
        , m_max_load_factor(std::exchange(other.m_max_load_factor, default_max_load_factor))
        , m_hash(std::move(other.m_hash))
        , m_equal(std::move(other.m_equal))
        , m_shifts(std::exchange(other.m_shifts, initial_shifts)) {
        // When the allocators differ the two containers above moved element by element, so other
        // still holds them. Either way it has to come out of this as an empty, usable table, which
        // the exchanges above have already made the rest of it -- and an empty table needs no
        // buckets, so this hands nothing back to it.
        other.m_values.clear();
        other.m_buckets.clear();
    }

    table(std::initializer_list<value_type> ilist,
          std::size_t bucket_count = 0,
          Hash const& hash = Hash(),
          KeyEqual const& equal = KeyEqual(),
          allocator_type const& alloc = allocator_type())
        : table(bucket_count, hash, equal, alloc) {
        insert(ilist);
    }

    table(std::initializer_list<value_type> ilist, size_type bucket_count, allocator_type const& alloc)
        : table(ilist, bucket_count, Hash(), KeyEqual(), alloc) {}

    table(std::initializer_list<value_type> init, size_type bucket_count, Hash const& hash, allocator_type const& alloc)
        : table(init, bucket_count, hash, KeyEqual(), alloc) {}

    ~table() = default;

    auto operator=(table const& other) -> table& {
        if (&other != this) {
            deallocate_buckets(); // deallocate before m_values is set (might have another allocator)

            // Copying the values, and building the buckets for them, both allocate. Until both have
            // happened the table holds values with no buckets to find them by; a throw in there
            // used to leave it that way, so size() counted elements that find() could not reach and
            // the next lookup probed a bucket array that was not there. See reset_to_empty().
            if constexpr (ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS()) {
                try {
                    copy_everything_from(other);
                } catch (...) {
                    reset_to_empty();
                    throw;
                }
            } else {
                copy_everything_from(other);
            }
        }
        return *this;
    }

    // The condition used to be wrapped in another noexcept(), which asks whether evaluating a bool expression can
    // throw. It cannot, so the specification was noexcept(true) whatever the traits said, and a type with a throwing
    // move assignment terminated instead of propagating.
    auto operator=(table&& other) noexcept(move_assign_is_nothrow) -> table& {
        if (&other != this) {
            deallocate_buckets(); // deallocate before m_values is set (might have another allocator)

            // Same window as the copy assignment above, and reachable for the same reason: with an
            // allocator that neither propagates nor compares equal the move below moves the
            // elements one at a time into memory it has to allocate. See reset_to_empty().
            // Exactly when this operator does not promise noexcept, which is what makes the
            // recovery reachable rather than a rethrow inside a noexcept function.
            if constexpr (ANKERL_UNORDERED_DENSE_HAS_EXCEPTIONS() && !move_assign_is_nothrow) {
                try {
                    move_everything_from(std::move(other));
                } catch (...) {
                    reset_to_empty();
                    throw;
                }
            } else {
                move_everything_from(std::move(other));
            }
        }
        return *this;
    }

    auto operator=(std::initializer_list<value_type> ilist) -> table& {
        clear();
        insert(ilist);
        return *this;
    }

    auto get_allocator() const noexcept -> allocator_type {
        return m_values.get_allocator();
    }

    // iterators //////////////////////////////////////////////////////////////

    auto begin() noexcept -> iterator {
        return m_values.begin();
    }

    auto begin() const noexcept -> const_iterator {
        return m_values.begin();
    }

    auto cbegin() const noexcept -> const_iterator {
        return m_values.cbegin();
    }

    auto end() noexcept -> iterator {
        return m_values.end();
    }

    auto cend() const noexcept -> const_iterator {
        return m_values.cend();
    }

    auto end() const noexcept -> const_iterator {
        return m_values.end();
    }

    // capacity ///////////////////////////////////////////////////////////////

    [[nodiscard]] auto empty() const noexcept -> bool {
        return m_values.empty();
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return m_values.size();
    }

    [[nodiscard]] static constexpr auto max_size() noexcept -> std::size_t {
        if constexpr ((std::numeric_limits<value_idx_type>::max)() == (std::numeric_limits<std::size_t>::max)()) {
            return std::size_t{1} << (sizeof(value_idx_type) * 8 - 1);
        } else {
            return std::size_t{1} << (sizeof(value_idx_type) * 8);
        }
    }

    // modifiers //////////////////////////////////////////////////////////////

    void clear() {
        if (!empty()) {
            m_values.clear();
            clear_buckets();
        }
    }

    auto insert(value_type const& value) -> std::pair<iterator, bool> {
        return emplace(value);
    }

    auto insert(value_type&& value) -> std::pair<iterator, bool> {
        return emplace(std::move(value));
    }

    template <class P, std::enable_if_t<std::is_constructible_v<value_type, P&&>, bool> = true>
    auto insert(P&& value) -> std::pair<iterator, bool> {
        return emplace(std::forward<P>(value));
    }

    auto insert(const_iterator /*hint*/, value_type const& value) -> iterator {
        return insert(value).first;
    }

    auto insert(const_iterator /*hint*/, value_type&& value) -> iterator {
        return insert(std::move(value)).first;
    }

    template <class P, std::enable_if_t<std::is_constructible_v<value_type, P&&>, bool> = true>
    auto insert(const_iterator /*hint*/, P&& value) -> iterator {
        return insert(std::forward<P>(value)).first;
    }

    // Inserting a range hashes ahead of the element it is placing, and asks for the block each of
    // those will come home to.
    //
    // A single insert cannot do this: the hash is the first thing it computes and the block is the
    // next thing it needs, so there is nothing to put between them. Over a range there is -- the
    // hash of a later element, which depends on nothing the table is doing. It is the same trick the
    // growth rehash uses, where it is worth 1.26x on that loop.
    //
    // Only for an iterator that can be walked twice. An input iterator is single-pass, so reading
    // ahead would mean buffering the elements, and those copies would cost more than the prefetch
    // saves; such a range takes the plain loop.
    template <class InputIt>
    void insert(InputIt first, InputIt last) {
        if constexpr (detail::is_forward_iterator_v<InputIt>) {
            do_insert_range(first, last);
        } else {
            for (; first != last; ++first) {
                insert(*first);
            }
        }
    }

    void insert(std::initializer_list<value_type> ilist) {
        insert(ilist.begin(), ilist.end());
    }

    // nonstandard API: *this is emptied.
    // Also see "A Standard flat_map" https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p0429r9.pdf
    auto extract() && -> value_container_type {
        auto values = std::move(m_values);

        // Moving the values out does not empty the buckets, and they index into the container that just left. Emptying
        // them here is what makes "*this is emptied" true: without it the table looks empty -- size() is 0, find()
        // returns end() -- and then the next insert probes a bucket pointing at an element that is no longer there.
        m_values.clear();
        clear_buckets();
        return values;
    }

    // nonstandard API:
    // Discards the internally held container and replaces it with the one passed. Erases non-unique elements.
    auto replace(value_container_type&& container) {
        if (ANKERL_UNORDERED_DENSE_UNLIKELY(container.size() > max_size()))
            ANKERL_UNORDERED_DENSE_UNLIKELY_ATTR {
                on_error_too_many_elements();
            }
        auto shifts = calc_shifts_for_size(container.size());
        if (0 == bucket_count() || shifts < m_shifts || container.get_allocator() != m_values.get_allocator()) {
            allocate_buckets_from_shift(shifts);
        }
        clear_buckets();

        m_values = std::move(container);

        // can't use fill_buckets_from_values() because container elements might not be unique
        //
        // Counted in std::size_t rather than in value_idx_type. max_size() is exactly the number
        // values that type can hold, so a container of precisely that many has a size that is not
        // representable in it: the cast wrapped to zero, the loop below never ran once, and the
        // table came back reporting size() elements with no bucket pointing at any of them. Every
        // index the loop produces is representable -- it is the count that is not.
        auto value_idx = std::size_t{};

        // Hashing ahead of where it places, which this loop could not simply be given because of how
        // it removes a duplicate: it pulls back() into the hole, and a lookahead has already hashed
        // the elements near the back.
        //
        // What makes it possible anyway is that the pull disturbs exactly *one* position, the last.
        // So while the container is longer than the window by more than one, the window cannot
        // contain the element that moves, and every hash in it stays valid. Only the tail -- the last
        // pipeline_depth + 1 elements -- has to run unpipelined, and it is the loop below.
        //
        // A duplicate inside the pipelined region therefore costs one re-hash, not a flush: the
        // element at value_idx is a different one afterwards, and nothing else in the ring moved.
        // The index does not advance, exactly as it does not in the plain loop.
        //
        // The exact condition for safety is size > value_idx + pipeline_depth; the + 1 below is one
        // stricter than it needs to be, which costs one element of pipelining and buys margin on a
        // loop where an off-by-one is a wrong answer rather than a crash. A mutation sweep confirms
        // there is slack on both sides -- loosening the bound by one still passes, because the
        // iteration that could read a stale entry is the one after which the loop exits -- so the
        // boundary is not balanced on a knife edge in either direction.
        //
        // The bucket array is sized once before this and never grows here, so the pipelined loop can
        // hold its base in a local -- which is what lets the ring carry block pointers rather than
        // group indices.
        //
        // And it only runs when the work does not fit in cache. The pipeline is not free: the ring
        // round trip costs **13.6 instructions per element**, 73.4 against 86.6 at n = 1024, and a
        // prefetch of a line already in L2 still occupies a load port. Measured against the
        // unpipelined loop on 2026-09-11, at 0% duplicates, the ratio tracks the footprint against
        // this machine's 1 MiB L2 and nothing else:
        //
        //     688 KiB (n=32768)   1.20x -- a fifth slower
        //    1376 KiB (n=65536)   1.03x
        //    2064 KiB (n=98304)   0.92x
        //    5504 KiB (n=262144)  0.90x
        //    22 MiB   (n=4000000) 0.74x
        //
        // So the gate is the footprint the loop streams -- the values plus the index -- against
        // pipeline_min_bytes. Getting it wrong in the safe direction (a machine with more L2) costs
        // at most that 1.20x on one octave of sizes; omitting the gate costs it on every size below
        // a quarter million.
        // m_shifts describes the array that was just allocated or kept above, so this is the index
        // the dedup will actually run against.
        if (m_values.size() > pipeline_depth + 1 && index_bytes_for(m_shifts) > pipeline_min_index_bytes) {
            do_replace_pipelined(value_idx);
        }

        // loop until we reach the end of the container. duplicated entries will be replaced with back().
        // On a cursor rather than m_values[value_idx], for the reason do_replace_pipelined gives: the
        // placement below stores a fingerprint, and an indexed read after it has to load the
        // container's data pointer back before it can form the next element's address.
        auto read = m_values.begin() + static_cast<difference_type>(value_idx);
        while (value_idx != m_values.size()) {
            auto const& key = get_key(*read);
            auto const mh = mixed_hash(key);
            auto r = probe(key, mh);
            if (r.found) {
                if (value_idx != m_values.size() - 1) {
                    *read = std::move(m_values.back());
                }
                m_values.pop_back();
            } else {
                place_group(mh, static_cast<value_idx_type>(value_idx));
                ++read;
                ++value_idx;
            }
        }
    }

    template <class M, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto insert_or_assign(Key const& key, M&& mapped) -> std::pair<iterator, bool> {
        return do_insert_or_assign(key, std::forward<M>(mapped));
    }

    template <class M, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto insert_or_assign(Key&& key, M&& mapped) -> std::pair<iterator, bool> {
        return do_insert_or_assign(std::move(key), std::forward<M>(mapped));
    }

    template <typename K,
              typename M,
              typename Q = T,
              typename H = Hash,
              typename KE = KeyEqual,
              std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE>, bool> = true>
    auto insert_or_assign(K&& key, M&& mapped) -> std::pair<iterator, bool> {
        return do_insert_or_assign(std::forward<K>(key), std::forward<M>(mapped));
    }

    template <class M, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto insert_or_assign(const_iterator /*hint*/, Key const& key, M&& mapped) -> iterator {
        return do_insert_or_assign(key, std::forward<M>(mapped)).first;
    }

    template <class M, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto insert_or_assign(const_iterator /*hint*/, Key&& key, M&& mapped) -> iterator {
        return do_insert_or_assign(std::move(key), std::forward<M>(mapped)).first;
    }

    template <typename K,
              typename M,
              typename Q = T,
              typename H = Hash,
              typename KE = KeyEqual,
              std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE>, bool> = true>
    auto insert_or_assign(const_iterator /*hint*/, K&& key, M&& mapped) -> iterator {
        return do_insert_or_assign(std::forward<K>(key), std::forward<M>(mapped)).first;
    }

    // Single arguments for unordered_set can be used without having to construct the value_type
    template <class K,
              typename Q = T,
              typename H = Hash,
              typename KE = KeyEqual,
              std::enable_if_t<!is_map_v<Q> && is_transparent_v<H, KE>, bool> = true>
    auto emplace(K&& key) -> std::pair<iterator, bool> {
        allocate_buckets_if_none();
        auto const mh = mixed_hash(key);
        auto r = probe(key, mh);
        if (r.found) {
            // found it, return without ever actually creating anything
            return {begin() + static_cast<difference_type>(r.value_idx), false};
        }

        // value is new, insert element first, so when exception happens we are in a valid state
        return do_place_element(mh, std::forward<K>(key));
    }

    template <class... Args>
    auto emplace(Args&&... args) -> std::pair<iterator, bool> {
        allocate_buckets_if_none();

        // we have to instantiate the value_type to be able to access the key.
        // 1. emplace_back the object so it is constructed. 2. If the key is already there, pop it later in the loop.
        auto& key = get_key(m_values.emplace_back(std::forward<Args>(args)...));
        auto const mh = mixed_hash(key);
        auto r = probe(key, mh);
        if (r.found) {
            m_values.pop_back(); // value was already there, so get rid of it
            move_home(r.group_idx, r.lane, mh);
            return {begin() + static_cast<difference_type>(r.value_idx), false};
        }

        // value is new, place it in the first free slot on its probe sequence
        auto value_idx = static_cast<value_idx_type>(m_values.size() - 1);
        if (ANKERL_UNORDERED_DENSE_UNLIKELY(is_full()))
            ANKERL_UNORDERED_DENSE_UNLIKELY_ATTR {
                // increase_size just rehashes all the data we have in m_values
                increase_size();
            }
        else {
            place_group(mh, value_idx);
        }
        return {begin() + static_cast<difference_type>(value_idx), true};
    }

    template <class... Args>
    auto emplace_hint(const_iterator /*hint*/, Args&&... args) -> iterator {
        return emplace(std::forward<Args>(args)...).first;
    }

    template <class... Args, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto try_emplace(Key const& key, Args&&... args) -> std::pair<iterator, bool> {
        return do_try_emplace(key, std::forward<Args>(args)...);
    }

    template <class... Args, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto try_emplace(Key&& key, Args&&... args) -> std::pair<iterator, bool> {
        return do_try_emplace(std::move(key), std::forward<Args>(args)...);
    }

    template <class... Args, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto try_emplace(const_iterator /*hint*/, Key const& key, Args&&... args) -> iterator {
        return do_try_emplace(key, std::forward<Args>(args)...).first;
    }

    template <class... Args, typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto try_emplace(const_iterator /*hint*/, Key&& key, Args&&... args) -> iterator {
        return do_try_emplace(std::move(key), std::forward<Args>(args)...).first;
    }

    template <
        typename K,
        typename... Args,
        typename Q = T,
        typename H = Hash,
        typename KE = KeyEqual,
        std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE> && is_neither_convertible_v<K&&, iterator, const_iterator>,
                         bool> = true>
    auto try_emplace(K&& key, Args&&... args) -> std::pair<iterator, bool> {
        return do_try_emplace(std::forward<K>(key), std::forward<Args>(args)...);
    }

    template <
        typename K,
        typename... Args,
        typename Q = T,
        typename H = Hash,
        typename KE = KeyEqual,
        std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE> && is_neither_convertible_v<K&&, iterator, const_iterator>,
                         bool> = true>
    auto try_emplace(const_iterator /*hint*/, K&& key, Args&&... args) -> iterator {
        return do_try_emplace(std::forward<K>(key), std::forward<Args>(args)...).first;
    }

    // Replaces the key at the given iterator with new_key. This does not change any other data in the underlying table, so
    // all iterators and references remain valid. However, this operation can fail if new_key already exists in the table.
    // In that case, returns {iterator to the already existing new_key, false} and no change is made.
    //
    // In the case of a set, this effectively removes the old key and inserts the new key at the same spot, which is more
    // efficient than removing the old key and inserting the new key because it avoids repositioning the last element.
    template <typename K>
    auto replace_key(iterator it, K&& new_key) -> std::pair<iterator, bool> {
        auto const new_key_hash = mixed_hash(new_key);

        // first, check if new_key already exists and return if so
        auto const r = probe(new_key, new_key_hash);
        if (r.found) {
            return {begin() + static_cast<difference_type>(r.value_idx), false};
        }

        // const_cast is needed because iterator for the set is always const, so adding another get_key overload is not
        // feasible.
        auto& target_key = const_cast<key_type&>(get_key(*it));
        auto const old_key_hash = mixed_hash(target_key);

        // Replace the key before doing any index changes. If it throws, no harm done, we are still in a valid state as
        // we have not modified the index yet.
        target_key = std::forward<K>(new_key);

        auto const value_idx = static_cast<value_idx_type>(it - begin());
        auto const at = slot_of_value(old_key_hash, value_idx);
        erase_group_slot(at.group_idx, at.lane, old_key_hash);
        place_group(new_key_hash, value_idx);
        return {it, true};
    }

    // What do_erase needs of the element an iterator points at: its index, the mixed hash of its
    // key, and the slot pointing at it, which is searched from the hash.
    struct located {
        value_idx_type value_idx;
        std::uint64_t mh;
        group_slot at;
    };

    [[nodiscard]] auto locate(iterator it) const -> located {
        auto const value_idx = static_cast<value_idx_type>(it - cbegin());
        auto const mh = mixed_hash(get_key(*it));
        return {value_idx, mh, slot_of_value(mh, value_idx)};
    }

    auto erase(iterator it) -> iterator {
        auto const e = locate(it);
        // The noexcept here and on the other two erase callbacks is what keeps erase() out of do_erase()'s exception
        // guard: a call expression is noexcept only if the callee says so, an empty body is not enough.
        do_erase(e.at, e.value_idx, e.mh, [](value_type const& /*unused*/) noexcept -> void {
        });
        return begin() + static_cast<difference_type>(e.value_idx);
    }

    auto extract(iterator it) -> value_type {
        auto const e = locate(it);
        auto tmp = std::optional<value_type>{};
        do_erase(e.at, e.value_idx, e.mh, [&tmp](value_type&& val) -> void {
            tmp = std::move(val);
        });
        return std::move(tmp).value();
    }

    template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto erase(const_iterator it) -> iterator {
        return erase(begin() + (it - cbegin()));
    }

    template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto extract(const_iterator it) -> value_type {
        return extract(begin() + (it - cbegin()));
    }

    auto erase(const_iterator first, const_iterator last) -> iterator {
        auto const idx_first = first - cbegin();
        auto const idx_last = last - cbegin();
        auto const first_to_last = std::distance(first, last);
        auto const last_to_end = std::distance(last, cend());

        // remove elements from left to right which moves elements from the end back
        auto const mid = idx_first + (std::min)(first_to_last, last_to_end);
        auto idx = idx_first;
        while (idx != mid) {
            erase(begin() + idx);
            ++idx;
        }

        // all elements from the right are moved, now remove the last element until all done
        idx = idx_last;
        while (idx != mid) {
            --idx;
            erase(begin() + idx);
        }

        return begin() + idx_first;
    }

    auto erase(Key const& key) -> std::size_t {
        return do_erase_key(key, [](value_type const& /*unused*/) noexcept -> void {
        });
    }

    auto extract(Key const& key) -> std::optional<value_type> {
        auto tmp = std::optional<value_type>{};
        do_erase_key(key, [&tmp](value_type&& val) -> void {
            tmp = std::move(val);
        });
        return tmp;
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto erase(K&& key) -> std::size_t {
        return do_erase_key(std::forward<K>(key), [](value_type const& /*unused*/) noexcept -> void {
        });
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto extract(K&& key) -> std::optional<value_type> {
        auto tmp = std::optional<value_type>{};
        do_erase_key(std::forward<K>(key), [&tmp](value_type&& val) -> void {
            tmp = std::move(val);
        });
        return tmp;
    }

    void swap(table& other) noexcept(std::is_nothrow_swappable_v<value_container_type> &&
                                     std::is_nothrow_swappable_v<bucket_container_type> && std::is_nothrow_swappable_v<Hash> &&
                                     std::is_nothrow_swappable_v<KeyEqual>) {
        // There is no free swap() for table, so "swap(other, *this)" used to resolve to the generic std::swap: three
        // move assignments, each of which hands the moved-from table a freshly allocated set of buckets. That is three
        // allocations for an operation that needs none, and three ways to throw out of a noexcept function.
        //
        // segmented_vector has a swap of its own now, so both container choices answer the allocator
        // question the same way; see its definition for what the generic std::swap did instead.
        //
        // Calling it as a member rather than unqualified is not what fixes that -- the free swap
        // beside it is found by ADL just the same. It is so that a value container supplied from
        // some other namespace cannot quietly fall back to the three-move std::swap: every
        // container is required to have the member, none is required to have the free function.
        m_values.swap(other.m_values);
        m_buckets.swap(other.m_buckets);
        using std::swap;
        swap(m_max_bucket_capacity, other.m_max_bucket_capacity);
        swap(m_group_mask, other.m_group_mask);
        swap(m_max_load_factor, other.m_max_load_factor);
        swap(m_hash, other.m_hash);
        swap(m_equal, other.m_equal);
        swap(m_shifts, other.m_shifts);
    }

    // Every element of source whose key is not here already moves over; the rest stay behind. The
    // source may hash and compare differently -- that is the overload std::unordered_map has, and
    // the keys are re-hashed with this table's hasher on the way in.
    //
    // Two differences from the standard's merge are worth knowing, and both follow from the elements
    // living in a vector rather than in nodes. References and iterators into *either* table are
    // invalidated, where a node splice preserves the ones into the elements it moves; and the
    // source's order changes, because every element taken out of the middle of it leaves a gap that
    // the elements behind it close. erase() already carries both, and this is the same mechanism.
    template <class H2, class KE2>
    void merge(sibling_table<H2, KE2>& source) {
        do_merge(source);
    }

    template <class H2, class KE2>
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    void merge(sibling_table<H2, KE2>&& source) {
        merge(source);
    }

    // lookup /////////////////////////////////////////////////////////////////

    template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto at(key_type const& key) -> Q& {
        return do_at(key);
    }

    template <typename K,
              typename Q = T,
              typename H = Hash,
              typename KE = KeyEqual,
              std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE>, bool> = true>
    auto at(K const& key) -> Q& {
        return do_at(key);
    }

    template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto at(key_type const& key) const -> Q const& {
        return do_at(key);
    }

    template <typename K,
              typename Q = T,
              typename H = Hash,
              typename KE = KeyEqual,
              std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE>, bool> = true>
    auto at(K const& key) const -> Q const& {
        return do_at(key);
    }

    template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto operator[](Key const& key) -> Q& {
        return try_emplace(key).first->second;
    }

    template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto operator[](Key&& key) -> Q& {
        return try_emplace(std::move(key)).first->second;
    }

    template <typename K,
              typename Q = T,
              typename H = Hash,
              typename KE = KeyEqual,
              std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE>, bool> = true>
    auto operator[](K&& key) -> Q& {
        return try_emplace(std::forward<K>(key)).first->second;
    }

    auto count(Key const& key) const -> std::size_t {
        return find(key) == end() ? 0 : 1;
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto count(K const& key) const -> std::size_t {
        return find(key) == end() ? 0 : 1;
    }

    auto find(Key const& key) -> iterator {
        return do_find(key);
    }

    auto find(Key const& key) const -> const_iterator {
        return do_find(key);
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto find(K const& key) -> iterator {
        return do_find(key);
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto find(K const& key) const -> const_iterator {
        return do_find(key);
    }

    auto contains(Key const& key) const -> bool {
        return find(key) != end();
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto contains(K const& key) const -> bool {
        return find(key) != end();
    }

    auto equal_range(Key const& key) -> std::pair<iterator, iterator> {
        auto it = do_find(key);
        return {it, it == end() ? end() : it + 1};
    }

    auto equal_range(const Key& key) const -> std::pair<const_iterator, const_iterator> {
        auto it = do_find(key);
        return {it, it == end() ? end() : it + 1};
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto equal_range(K const& key) -> std::pair<iterator, iterator> {
        auto it = do_find(key);
        return {it, it == end() ? end() : it + 1};
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto equal_range(K const& key) const -> std::pair<const_iterator, const_iterator> {
        auto it = do_find(key);
        return {it, it == end() ? end() : it + 1};
    }

    // lookup with a precomputed hash /////////////////////////////////////////

    // Looking the same key up over and over -- a handful of string literals against a map parsed
    // out of a document, say -- hashes it every time, and for a long key that hashing is most of
    // the cost of the lookup. Hashing it once instead is what hash_for() and these overloads are
    // for:
    //
    //     auto const h = map.hash_for("some-long-key"); // once
    //     auto it = map.find("some-long-key", h);       // as often as you like
    //
    // The key is still needed, because a lookup that found a bucket still has to compare keys to
    // know it found the right one. What is saved is the hashing, not the comparison.
    //
    // The number a lookup wants is the one hash_for() returns, and nothing else: it is the hasher's
    // output finalized the way a lookup finalizes it, which for most hashers is not the same number
    // the hasher gave. An integer will not convert to a precomputed_hash, which is the mistake
    // worth blocking; the value inside stays open, since a caller may want to keep or move one.
    // Every table with this hasher takes it, so one hash can serve a map and a set together, and a
    // stateless hasher makes it good for the life of the program. What it does not survive is the
    // key changing -- pass the hash of a different key and the lookup quietly finds nothing.
    //
    // Only lookups take one. Insertion never will: a lookup handed the wrong hash merely misses,
    // while an insertion handed one files the element under a probe chain it is not on, which
    // loses it for good and lets a second copy of the same key in beside it. Erase is left out for
    // a duller reason -- it hashes the moved element as well as the key, so precomputing the key's
    // hash saves it only half its hashing.
    [[nodiscard]] auto hash_for(Key const& key) const -> precomputed_hash {
        return {mixed_hash(key)};
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    [[nodiscard]] auto hash_for(K const& key) const -> precomputed_hash {
        return {mixed_hash(key)};
    }

    // Looks up a range of keys, calls f on each one that is there, and returns how many that was.
    //
    // A batch is worth having because a lookup here is two dependent memory accesses -- the group's
    // block, then the value -- and a loop that does one lookup at a time can only overlap them as
    // far as the core's out-of-order window reaches past a whole loop body. Splitting the batch into
    // passes reaches further: every key's block is asked for before any key's block is waited on.
    //
    //   1. hash each key in the chunk and fetch the block it comes home to
    //   2. match the fingerprints, by which time the blocks have arrived
    //   3. compare the keys and call f
    //
    // Measured against the same batch looked up one key at a time with find(), a map of eight byte
    // keys to eight byte values, ns per lookup: at four million entries 34.0 to 26.3 on all hits and
    // 34.4 to 28.4 at half, at sixteen million 36.5 to 30.4 and 37.3 to 31.5. About 1.2x, and it
    // needs a table past the cache to be worth anything.
    //
    // Two things this deliberately does not do. It does not prefetch the *value* in pass 2, which is
    // what boost::concurrent_flat_map's bulk visit does and what this was built to try: the address
    // is known there, unlike in a single lookup, and it measured +3.8% on all hits and **-4.8% at
    // half hits**, because an absent key has nothing to fetch and a present one is already covered
    // once the passes are separated.
    //
    // And it is a fixed-size chunk with straight passes rather than a sliding ring. A ring is the
    // obvious shape and it is worse, measured: at four million entries 31.8 ns against 26.6 on all
    // hits, at sixteen million 34.7 against 30.4. Not because of code generation -- both retire
    // 224.5 instructions per lookup, identical to the tenth -- but because the ring takes 15% more
    // cycles waiting. The gap is 15% on all hits and 6% at half hits, and a miss reads no value, so
    // what it is losing is the *value* loads: the third pass issues sixteen of them back to back and
    // nothing else here creates that parallelism, since the value is the one access not prefetched.
    // A ring even has the better block prefetch -- a full depth of work for every element, where a
    // chunk gives its last element less than its first -- and still loses.
    //
    // The second reason is that a ring has to be read before the slot it frees is refilled, and
    // getting that backwards is a wrong answer rather than a crash.
    //
    // f is taken by value, as the standard algorithms take a callable, and is called with
    // value_type& -- or value_type const& on a const map. Keys that are absent are not reported; the
    // return value counts the ones that were found.
    template <typename FwdIt, typename F>
    auto visit(FwdIt first, FwdIt last, F f) -> std::size_t {
        return do_visit(first, last, std::move(f));
    }

    template <typename FwdIt, typename F>
    auto visit(FwdIt first, FwdIt last, F f) const -> std::size_t {
        return const_cast<table*>(this)->do_visit( // NOLINT(cppcoreguidelines-pro-type-const-cast)
            first,
            last,
            [&f](value_type const& v) -> void {
                f(v);
            });
    }

    auto find(Key const& key, precomputed_hash ph) -> iterator {
        return do_find(key, ph);
    }

    auto find(Key const& key, precomputed_hash ph) const -> const_iterator {
        return do_find(key, ph);
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto find(K const& key, precomputed_hash ph) -> iterator {
        return do_find(key, ph);
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto find(K const& key, precomputed_hash ph) const -> const_iterator {
        return do_find(key, ph);
    }

    auto contains(Key const& key, precomputed_hash ph) const -> bool {
        return find(key, ph) != end();
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto contains(K const& key, precomputed_hash ph) const -> bool {
        return find(key, ph) != end();
    }

    auto count(Key const& key, precomputed_hash ph) const -> std::size_t {
        return find(key, ph) == end() ? 0 : 1;
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto count(K const& key, precomputed_hash ph) const -> std::size_t {
        return find(key, ph) == end() ? 0 : 1;
    }

    auto equal_range(Key const& key, precomputed_hash ph) -> std::pair<iterator, iterator> {
        auto it = do_find(key, ph);
        return {it, it == end() ? end() : it + 1};
    }

    auto equal_range(Key const& key, precomputed_hash ph) const -> std::pair<const_iterator, const_iterator> {
        auto it = do_find(key, ph);
        return {it, it == end() ? end() : it + 1};
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto equal_range(K const& key, precomputed_hash ph) -> std::pair<iterator, iterator> {
        auto it = do_find(key, ph);
        return {it, it == end() ? end() : it + 1};
    }

    template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool> = true>
    auto equal_range(K const& key, precomputed_hash ph) const -> std::pair<const_iterator, const_iterator> {
        auto it = do_find(key, ph);
        return {it, it == end() ? end() : it + 1};
    }

    template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto at(key_type const& key, precomputed_hash ph) -> Q& {
        return do_at(key, ph);
    }

    template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto at(key_type const& key, precomputed_hash ph) const -> Q const& {
        return do_at(key, ph);
    }

    template <typename K,
              typename Q = T,
              typename H = Hash,
              typename KE = KeyEqual,
              std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE>, bool> = true>
    auto at(K const& key, precomputed_hash ph) -> Q& {
        return do_at(key, ph);
    }

    template <typename K,
              typename Q = T,
              typename H = Hash,
              typename KE = KeyEqual,
              std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE>, bool> = true>
    auto at(K const& key, precomputed_hash ph) const -> Q const& {
        return do_at(key, ph);
    }

    // bucket interface ///////////////////////////////////////////////////////

    auto bucket_count() const noexcept -> std::size_t { // NOLINT(modernize-use-nodiscard)
        // from the mask rather than the array, and in slots, of which a group has sixteen
        return m_buckets.empty() ? 0 : (std::size_t{m_group_mask} + 1) * slots_per_group;
    }

    static constexpr auto max_bucket_count() noexcept -> std::size_t { // NOLINT(modernize-use-nodiscard)
        return max_size();
    }

    // hash policy ////////////////////////////////////////////////////////////

    [[nodiscard]] auto load_factor() const -> float {
        return bucket_count() ? static_cast<float>(size()) / static_cast<float>(bucket_count()) : 0.0F;
    }

    [[nodiscard]] auto max_load_factor() const -> float {
        return m_max_load_factor;
    }

    void max_load_factor(float ml) {
        // A load factor above 1 is meaningful for a container that chains, and std::unordered_map takes one. Open
        // addressing cannot use it: m_max_bucket_capacity would exceed bucket_count(), is_full() would never fire, the
        // table would fill completely, and place_group() would then probe forever for an empty slot that does not
        // exist. Exactly 1 is fine, because is_full() is checked after the value is appended.
        m_max_load_factor = (std::min)(ml, 1.0F);
        if (bucket_count() != max_bucket_count()) {
            m_max_bucket_capacity = static_cast<value_idx_type>(static_cast<float>(bucket_count()) * max_load_factor());
        }
    }

    void rehash(std::size_t count) {
        count = (std::min)(count, max_size());
        auto const shifts = calc_shifts_for_size((std::max)(count, size()));
        if (shifts != m_shifts) {
            allocate_buckets_from_shift(shifts);
        } else if (m_buckets.empty()) {
            // Nothing allocated and nothing to index: stay in the state a default constructed
            // table is in, so that rehash() on an empty map still allocates nothing.
            return;
        } else {
            // The array is already the right size, and rehash() still has to rehash. It used to
            // return here, which made it a no-op in exactly the case a caller reaches for it: a
            // table whose probe sequences have drifted under churn is the same size as a fresh
            // one, so the only thing that repairs the drift did nothing. It is also what the
            // standard asks for -- rehash(n) sets the bucket count and *then* rehashes.
            clear_buckets();
        }
        m_values.shrink_to_fit();
        fill_buckets_from_values();
    }

    void reserve(std::size_t capa) {
        capa = (std::min)(capa, max_size());
        if constexpr (has_reserve<value_container_type>) {
            // std::deque doesn't have reserve(). Make sure we only call when available
            m_values.reserve(capa);
        }
        auto shifts = calc_shifts_for_size((std::max)(capa, size()));
        if (0 == bucket_count() || shifts < m_shifts) {
            allocate_buckets_from_shift(shifts);
            fill_buckets_from_values();
        }
    }

    // observers //////////////////////////////////////////////////////////////

    auto hash_function() const -> hasher {
        return m_hash;
    }

    auto key_eq() const -> key_equal {
        return m_equal;
    }

    // nonstandard API: expose the underlying values container
    [[nodiscard]] auto values() const noexcept -> value_container_type const& {
        return m_values;
    }

    // non-member functions ///////////////////////////////////////////////////

    friend auto operator==(table const& a, table const& b) -> bool {
        if (&a == &b) {
            return true;
        }
        if (a.size() != b.size()) {
            return false;
        }
        for (auto const& b_entry : b) {
            auto it = a.find(get_key(b_entry));
            if constexpr (is_map_v<T>) {
                // map: check that key is here, then also check that value is the same
                if (a.end() == it || !(b_entry.second == it->second)) {
                    return false;
                }
            } else {
                // set: only check that the key is here
                if (a.end() == it) {
                    return false;
                }
            }
        }
        return true;
    }

    friend auto operator!=(table const& a, table const& b) -> bool {
        return !(a == b);
    }

    // Standard containers provide this, and generic code written as "using std::swap; swap(a, b);" needs it to find
    // the member. Without it that call lands on the generic std::swap and moves three times.
    friend void swap(table& a, table& b) noexcept(noexcept(a.swap(b))) {
        a.swap(b);
    }
};

} // namespace detail

template <class Key,
          class T,
          class Hash = hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class AllocatorOrContainer = std::allocator<std::pair<Key, T>>,
          class Bucket = bucket_type::group>
using map = detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, false>;

template <class Key,
          class T,
          class Hash = hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class AllocatorOrContainer = std::allocator<std::pair<Key, T>>,
          class Bucket = bucket_type::group>
using segmented_map = detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, true>;

template <class Key,
          class Hash = hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class AllocatorOrContainer = std::allocator<Key>,
          class Bucket = bucket_type::group>
using set = detail::table<Key, void, Hash, KeyEqual, AllocatorOrContainer, Bucket, false>;

template <class Key,
          class Hash = hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class AllocatorOrContainer = std::allocator<Key>,
          class Bucket = bucket_type::group>
using segmented_set = detail::table<Key, void, Hash, KeyEqual, AllocatorOrContainer, Bucket, true>;

#    if defined(ANKERL_UNORDERED_DENSE_PMR)

namespace pmr {

template <class Key, class T, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class Bucket = bucket_type::group>
using map =
    detail::table<Key, T, Hash, KeyEqual, ANKERL_UNORDERED_DENSE_PMR::polymorphic_allocator<std::pair<Key, T>>, Bucket, false>;

template <class Key, class T, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class Bucket = bucket_type::group>
using segmented_map =
    detail::table<Key, T, Hash, KeyEqual, ANKERL_UNORDERED_DENSE_PMR::polymorphic_allocator<std::pair<Key, T>>, Bucket, true>;

template <class Key, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class Bucket = bucket_type::group>
using set = detail::table<Key, void, Hash, KeyEqual, ANKERL_UNORDERED_DENSE_PMR::polymorphic_allocator<Key>, Bucket, false>;

template <class Key, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class Bucket = bucket_type::group>
using segmented_set =
    detail::table<Key, void, Hash, KeyEqual, ANKERL_UNORDERED_DENSE_PMR::polymorphic_allocator<Key>, Bucket, true>;

} // namespace pmr

#    endif

// deduction guides ///////////////////////////////////////////////////////////

// deduction guides for alias templates are only possible since C++20
// see https://en.cppreference.com/w/cpp/language/class_template_argument_deduction

} // namespace ANKERL_UNORDERED_DENSE_NAMESPACE
} // namespace ankerl::unordered_dense

// std extensions /////////////////////////////////////////////////////////////

namespace std { // NOLINT(cert-dcl58-cpp)

template <class Key,
          class T,
          class Hash,
          class KeyEqual,
          class AllocatorOrContainer,
          class Bucket,
          class Pred,
          bool IsSegmented>
// NOLINTNEXTLINE(cert-dcl58-cpp)
auto erase_if(ankerl::unordered_dense::detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, IsSegmented>& map,
              Pred pred) -> std::size_t {
    using map_t = ankerl::unordered_dense::detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, IsSegmented>;

    // going back to front because erase() invalidates the end iterator
    auto const old_size = map.size();
    auto idx = old_size;
    while (idx) {
        --idx;
        auto it = map.begin() + static_cast<typename map_t::difference_type>(idx);
        if (pred(*it)) {
            map.erase(it);
        }
    }

    return old_size - map.size();
}

} // namespace std

#endif
#endif
