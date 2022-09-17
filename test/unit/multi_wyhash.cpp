#include <doctest.h>

#include <third-party/nanobench.h>

#include <array>
#include <cstdint>
#include <cstring>

#if defined(_MSC_VER) && defined(_M_X64)
#    include <intrin.h>
#    pragma intrinsic(_umul128)
#endif

#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#    define ANKERL_UNORDERED_DENSE_LIKELY(x) __builtin_expect(x, 1)   // NOLINT(cppcoreguidelines-macro-usage)
#    define ANKERL_UNORDERED_DENSE_UNLIKELY(x) __builtin_expect(x, 0) // NOLINT(cppcoreguidelines-macro-usage)
#else
#    define ANKERL_UNORDERED_DENSE_LIKELY(x) (x)   // NOLINT(cppcoreguidelines-macro-usage)
#    define ANKERL_UNORDERED_DENSE_UNLIKELY(x) (x) // NOLINT(cppcoreguidelines-macro-usage)
#endif

namespace wyhash {

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
    std::memcpy(&v, p, 8U);
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

[[maybe_unused]] [[nodiscard]] static inline auto hash(void const* key, size_t len) -> uint64_t {
    static constexpr auto secret = std::array{UINT64_C(0xa0761d6478bd642f),
                                              UINT64_C(0xe7037ed1a0b428db),
                                              UINT64_C(0x8ebc6af09c88c6e3),
                                              UINT64_C(0x589965cc75374cc3)};

    auto const* p = static_cast<uint8_t const*>(key);
    uint64_t seed = secret[0];
    uint64_t a{};
    uint64_t b{};
    if (ANKERL_UNORDERED_DENSE_LIKELY(len <= 16)) {
        if (ANKERL_UNORDERED_DENSE_LIKELY(len >= 4)) {
            a = (r4(p) << 32U) | r4(p + ((len >> 3U) << 2U));
            b = (r4(p + len - 4) << 32U) | r4(p + len - 4 - ((len >> 3U) << 2U));
        } else if (ANKERL_UNORDERED_DENSE_LIKELY(len > 0)) {
            a = r3(p, len);
            b = 0;
        } else {
            a = 0;
            b = 0;
        }
    } else {
        size_t i = len;
        if (ANKERL_UNORDERED_DENSE_UNLIKELY(i > 48)) {
            uint64_t see1 = seed;
            uint64_t see2 = seed;
            do {
                seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
                see1 = mix(r8(p + 16) ^ secret[2], r8(p + 24) ^ see1);
                see2 = mix(r8(p + 32) ^ secret[3], r8(p + 40) ^ see2);
                p += 48;
                i -= 48;
            } while (ANKERL_UNORDERED_DENSE_LIKELY(i > 48));
            seed ^= see1 ^ see2;
        }
        while (ANKERL_UNORDERED_DENSE_UNLIKELY(i > 16)) {
            seed = mix(r8(p) ^ secret[1], r8(p + 8) ^ seed);
            i -= 16;
            p += 16;
        }
        a = r8(p + i - 16);
        b = r8(p + i - 8);
    }

    return mix(secret[1] ^ len, mix(a ^ secret[1], b ^ seed));
}

} // namespace wyhash

template <typename... Args>
auto hash_multi(Args&&... args) -> uint64_t {
    auto ary = std::array<uint64_t, sizeof...(args)>{static_cast<uint64_t>(args)...};
    return wyhash::hash(ary.data(), ary.size() * sizeof(uint64_t));
}

TEST_CASE("wyhash_multiarg") {
    auto rng = ankerl::nanobench::Rng();
    auto a = std::array<uint64_t, 16>();
    for (auto& x : a) {
        x = rng();
    }

    REQUIRE(hash_multi(a[0]) == wyhash::hash(a.data(), sizeof(uint64_t) * 1));
    REQUIRE(hash_multi(a[0], a[1]) == wyhash::hash(a.data(), sizeof(uint64_t) * 2));
    REQUIRE(hash_multi(a[0], a[1], a[2]) == wyhash::hash(a.data(), sizeof(uint64_t) * 3));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3]) == wyhash::hash(a.data(), sizeof(uint64_t) * 4));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4]) == wyhash::hash(a.data(), sizeof(uint64_t) * 5));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4], a[5]) == wyhash::hash(a.data(), sizeof(uint64_t) * 6));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4], a[5], a[6]) == wyhash::hash(a.data(), sizeof(uint64_t) * 7));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]) == wyhash::hash(a.data(), sizeof(uint64_t) * 8));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8]) == wyhash::hash(a.data(), sizeof(uint64_t) * 9));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]) ==
            wyhash::hash(a.data(), sizeof(uint64_t) * 10));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10]) ==
            wyhash::hash(a.data(), sizeof(uint64_t) * 11));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11]) ==
            wyhash::hash(a.data(), sizeof(uint64_t) * 12));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12]) ==
            wyhash::hash(a.data(), sizeof(uint64_t) * 13));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13]) ==
            wyhash::hash(a.data(), sizeof(uint64_t) * 14));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14]) ==
            wyhash::hash(a.data(), sizeof(uint64_t) * 15));
    REQUIRE(hash_multi(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]) ==
            wyhash::hash(a.data(), sizeof(uint64_t) * 16));
}
