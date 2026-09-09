// Two hashes that ship as Rust crates, ported to C++ so they can run in the same process as the
// rest of scripts/ab/hash_others.cpp.
//
// Both ports are checked rather than trusted. gxhash reproduces the three `is_stable` vectors from
// its own test module (`gxhash32` of 0, 1 and 1000 zero bytes is 2533353535, 4243413987 and
// 2401749549), and foldhash is bit-identical to the crate at eleven lengths from 0 to 300 bytes,
// for both the fast and the quality variant, checked against a `cargo run` of the real thing.
#ifndef UDM_AB_HASH_PORTS_H
#define UDM_AB_HASH_PORTS_H

#include <cstddef>
#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------- foldhash, orlp, 77d8e3d
//
// src/lib.rs and src/fast.rs. Hashing a string through `BuildHasher::hash_one` with the crate's
// `nightly` feature is `write(bytes)` and then `finish()`, and `finish()` on an empty sponge returns
// the accumulator, so the two functions below are the whole of it. The per-hasher seed is
// `with_seed(0)`, which the crate defines as `ARBITRARY3`.
//
// Two things in here are the design and are worth reading: `hash_bytes_long` is `#[cold]
// #[inline(never)]` upstream, so every key over 16 bytes is an out-of-line call; and `write` rotates
// the accumulator by the length, which is how a length reaches the hash without costing a multiply.
namespace udm_ab::foldhash {

inline constexpr std::uint64_t arb0 = 0x243f6a8885a308d3ULL;
inline constexpr std::uint64_t arb1 = 0x13198a2e03707344ULL;
inline constexpr std::uint64_t arb3 = 0x082efa98ec4e6c89ULL;
inline constexpr std::uint64_t arb5 = 0xbe5466cf34e90c6cULL;

constexpr auto folded_multiply(std::uint64_t x, std::uint64_t y) -> std::uint64_t {
    auto const full = static_cast<__uint128_t>(x) * y;
    return static_cast<std::uint64_t>(full) ^ static_cast<std::uint64_t>(full >> 64U);
}

constexpr auto rotr(std::uint64_t x, unsigned r) -> std::uint64_t {
    r &= 63U;
    return r == 0 ? x : ((x >> r) | (x << (64U - r)));
}

struct seeds_t {
    std::uint64_t s[6];
};

// SharedSeed::from_u64: three folded multiplies per seed, then three bits forced on so that a seed
// is never zero, which is the weak point of a multiply-mix.
constexpr auto make_seeds(std::uint64_t seed) -> seeds_t {
    auto mix3 = [](std::uint64_t x) {
        return folded_multiply(folded_multiply(folded_multiply(x, arb5), arb5), arb5);
    };
    constexpr std::uint64_t forced_ones = (1ULL << 63U) | (1ULL << 31U) | 1ULL;
    auto const a = mix3(seed);
    auto const b = mix3(a);
    auto const c = mix3(b);
    auto const d = mix3(c);
    auto const e = mix3(d);
    auto const f = mix3(e);
    return {{a | forced_ones, b | forced_ones, c | forced_ones, d | forced_ones, e | forced_ones, f | forced_ones}};
}

inline constexpr auto seeds = make_seeds(0x1234567890abcdefULL);

inline auto ld(std::uint8_t const* p, std::size_t off) -> std::uint64_t {
    auto v = std::uint64_t{};
    std::memcpy(&v, p + off, 8U);
    return v;
}

inline auto hash_bytes_short(std::uint8_t const* b, std::size_t len, std::uint64_t acc) -> std::uint64_t {
    auto s0 = acc;
    auto s1 = seeds.s[1];
    if (len >= 8) {
        s0 ^= ld(b, 0);
        s1 ^= ld(b, len - 8);
    } else if (len >= 4) {
        auto x = std::uint32_t{};
        auto y = std::uint32_t{};
        std::memcpy(&x, b, 4U);
        std::memcpy(&y, b + len - 4, 4U);
        s0 ^= x;
        s1 ^= y;
    } else if (len > 0) {
        s0 ^= b[0];
        s1 ^= (static_cast<std::uint64_t>(b[len - 1]) << 8U) | b[len / 2];
    }
    return folded_multiply(s0, s1);
}

__attribute__((noinline)) inline auto hash_bytes_long(std::uint8_t const* v, std::size_t len, std::uint64_t acc)
    -> std::uint64_t {
    auto s0 = acc;
    auto s1 = acc + seeds.s[1];
    if (len > 128) {
        auto s2 = s0 + seeds.s[2];
        auto s3 = s0 + seeds.s[3];
        if (len > 256) {
            auto s4 = s0 + seeds.s[4];
            auto s5 = s0 + seeds.s[5];
            for (;;) {
                s0 = folded_multiply(ld(v, 0) ^ s0, ld(v, 48) ^ seeds.s[0]);
                s1 = folded_multiply(ld(v, 8) ^ s1, ld(v, 56) ^ seeds.s[0]);
                s2 = folded_multiply(ld(v, 16) ^ s2, ld(v, 64) ^ seeds.s[0]);
                s3 = folded_multiply(ld(v, 24) ^ s3, ld(v, 72) ^ seeds.s[0]);
                s4 = folded_multiply(ld(v, 32) ^ s4, ld(v, 80) ^ seeds.s[0]);
                s5 = folded_multiply(ld(v, 40) ^ s5, ld(v, 88) ^ seeds.s[0]);
                v += 96;
                len -= 96;
                if (len <= 256) {
                    break;
                }
            }
            s0 ^= s4;
            s1 ^= s5;
        }
        for (;;) {
            s0 = folded_multiply(ld(v, 0) ^ s0, ld(v, 32) ^ seeds.s[0]);
            s1 = folded_multiply(ld(v, 8) ^ s1, ld(v, 40) ^ seeds.s[0]);
            s2 = folded_multiply(ld(v, 16) ^ s2, ld(v, 48) ^ seeds.s[0]);
            s3 = folded_multiply(ld(v, 24) ^ s3, ld(v, 56) ^ seeds.s[0]);
            v += 64;
            len -= 64;
            if (len <= 128) {
                break;
            }
        }
        s0 ^= s2;
        s1 ^= s3;
    }
    s0 = folded_multiply(ld(v, 0) ^ s0, ld(v, len - 16) ^ seeds.s[0]);
    s1 = folded_multiply(ld(v, 8) ^ s1, ld(v, len - 8) ^ seeds.s[0]);
    if (len >= 32) {
        s0 = folded_multiply(ld(v, 16) ^ s0, ld(v, len - 32) ^ seeds.s[0]);
        s1 = folded_multiply(ld(v, 24) ^ s1, ld(v, len - 24) ^ seeds.s[0]);
        if (len >= 64) {
            s0 = folded_multiply(ld(v, 32) ^ s0, ld(v, len - 48) ^ seeds.s[0]);
            s1 = folded_multiply(ld(v, 40) ^ s1, ld(v, len - 40) ^ seeds.s[0]);
            if (len >= 96) {
                s0 = folded_multiply(ld(v, 48) ^ s0, ld(v, len - 64) ^ seeds.s[0]);
                s1 = folded_multiply(ld(v, 56) ^ s1, ld(v, len - 56) ^ seeds.s[0]);
            }
        }
    }
    return s0 ^ s1;
}

[[nodiscard]] inline auto fast(void const* key, std::size_t len) -> std::uint64_t {
    auto const* b = static_cast<std::uint8_t const*>(key);
    auto const acc = rotr(arb3, static_cast<unsigned>(len));
    return len <= 16 ? hash_bytes_short(b, len, acc) : hash_bytes_long(b, len, acc);
}

// The quality variant is the fast one plus one more folded multiply, and that one multiply is the
// difference between failing a strict avalanche test and passing it.
[[nodiscard]] inline auto quality(void const* key, std::size_t len) -> std::uint64_t {
    return folded_multiply(fast(key, len), arb0);
}

} // namespace udm_ab::foldhash

// ---------------------------------------------------------------------------- gxhash, ogxd, 55bde47
//
// src/gxhash/mod.rs and src/gxhash/platform/x86.rs, the non-hybrid 128 bit path, seed 0. Needs
// -maes, which is the first reason it cannot be a library default.
#if defined(__AES__)
#    include <immintrin.h>

namespace udm_ab::gxhash {

inline constexpr std::uint32_t keys[12] = {0xF2784542, 0xB09D3E21, 0x89C222E5, 0xFC3BC28E,
                                           0x03FCE279, 0xCB6B2E9B, 0xB361DC58, 0x39132BD9,
                                           0xD0012E32, 0x689D2B7D, 0x5544B1B7, 0xC78B122B};

inline auto ld(std::uint32_t const* p) -> __m128i {
    return _mm_loadu_si128(reinterpret_cast<__m128i const*>(p));
}
inline auto ldv(std::uint8_t const* p) -> __m128i {
    return _mm_loadu_si128(reinterpret_cast<__m128i const*>(p));
}
inline auto enc(__m128i d, __m128i k) -> __m128i {
    return _mm_aesenc_si128(d, k);
}
inline auto enc_last(__m128i d, __m128i k) -> __m128i {
    return _mm_aesenclast_si128(d, k);
}

inline auto partial_safe(std::uint8_t const* p, std::size_t len) -> __m128i {
    std::int8_t buf[16] = {};
    std::memcpy(buf, p, len);
    return _mm_add_epi8(_mm_loadu_si128(reinterpret_cast<__m128i const*>(buf)), _mm_set1_epi8(static_cast<char>(len)));
}

// Reads a whole vector and masks it, which is only legal when the read cannot cross a page.
inline auto partial_unsafe(std::uint8_t const* p, std::size_t len) -> __m128i {
    auto const idx = _mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    auto const lv = _mm_set1_epi8(static_cast<char>(len));
    return _mm_add_epi8(_mm_and_si128(ldv(p), _mm_cmpgt_epi8(lv, idx)), lv);
}

inline auto partial(std::uint8_t const* p, std::size_t len) -> __m128i {
    return (reinterpret_cast<std::uintptr_t>(p) & 4095U) < 4096U - 16U ? partial_unsafe(p, len)
                                                                       : partial_safe(p, len);
}

inline auto compress_8(std::uint8_t const* p, std::uintptr_t end, __m128i hv, std::size_t len) -> __m128i {
    auto t1 = _mm_setzero_si128();
    auto t2 = _mm_setzero_si128();
    auto l1 = hv;
    auto l2 = hv;
    while (reinterpret_cast<std::uintptr_t>(p) < end) {
        auto const a = enc(enc(enc(ldv(p), ldv(p + 32)), ldv(p + 64)), ldv(p + 96));
        auto const b = enc(enc(enc(ldv(p + 16), ldv(p + 48)), ldv(p + 80)), ldv(p + 112));
        p += 128;
        t1 = _mm_add_epi8(t1, ld(keys));
        t2 = _mm_add_epi8(t2, ld(keys + 4));
        l1 = enc_last(enc(a, t1), l1);
        l2 = enc_last(enc(b, t2), l2);
    }
    auto const lv = _mm_set1_epi32(static_cast<int>(len));
    return enc(_mm_add_epi8(l1, lv), _mm_add_epi8(l2, lv));
}

inline auto compress_all(std::uint8_t const* p, std::size_t len) -> __m128i {
    if (len == 0) {
        return _mm_setzero_si128();
    }
    if (len <= 16) {
        return partial(p, len);
    }
    auto const end = reinterpret_cast<std::uintptr_t>(p) + len;
    auto const extra = len % 16;
    auto hv = __m128i();
    if (extra == 0) {
        hv = ldv(p);
        p += 16;
    } else {
        hv = partial_unsafe(p, extra);
        p += extra;
    }
    auto v0 = ldv(p);
    p += 16;
    if (len > 32) {
        v0 = enc(v0, ldv(p));
        p += 16;
        if (len > 48) {
            v0 = enc(v0, ldv(p));
            p += 16;
            if (len > 64) {
                auto const rem = end - reinterpret_cast<std::uintptr_t>(p);
                auto const blocks = rem / (16U * 8U) * 8U;
                auto const stop = reinterpret_cast<std::uintptr_t>(p) + (rem - blocks * 16U) / 16U * 16U;
                while (reinterpret_cast<std::uintptr_t>(p) < stop) {
                    hv = enc(hv, ldv(p));
                    p += 16;
                }
                hv = compress_8(p, end, hv, len);
            }
        }
    }
    return enc_last(hv, enc(enc(v0, ld(keys)), ld(keys + 4)));
}

[[nodiscard]] inline auto hash(void const* key, std::size_t len) -> std::uint64_t {
    auto h = enc(compress_all(static_cast<std::uint8_t const*>(key), len), _mm_set1_epi64x(0));
    h = enc(h, ld(keys));
    h = enc(h, ld(keys + 4));
    h = enc_last(h, ld(keys + 8));
    return static_cast<std::uint64_t>(_mm_cvtsi128_si64(h));
}

} // namespace udm_ab::gxhash
#endif

#endif
