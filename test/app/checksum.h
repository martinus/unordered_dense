#pragma once

#include <app/Counter.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace checksum {

// final step from MurmurHash3
[[nodiscard]] constexpr auto mix(uint64_t k) -> uint64_t {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

[[nodiscard]] constexpr auto mix(std::string_view data) -> uint64_t {
    constexpr uint64_t FNV_offset_basis = UINT64_C(14695981039346656037);
    constexpr uint64_t FNV_prime = UINT64_C(1099511628211);

    uint64_t val = FNV_offset_basis;
    for (auto c : data) {
        val ^= static_cast<uint64_t>(c);
        val *= FNV_prime;
    }
    return val;
}

[[nodiscard]] auto mix(Counter::Obj const& cdv) -> uint64_t {
    return mix(cdv.get());
}

// from boost::hash_combine, with additional fmix64 of value
[[nodiscard]] constexpr auto combine(uint64_t seed, uint64_t value) -> uint64_t {
    return seed ^ (value + 0x9e3779b9 + (seed << 6U) + (seed >> 2U));
}

// calculates a hash of any iterable map. Order is irrelevant for the hash's result, as it simply
// xors the elements together.
template <typename M>
[[nodiscard]] constexpr auto map(const M& map) -> uint64_t {
    uint64_t combined_hash = 1;

    uint64_t numElements = 0;
    for (auto const& entry : map) {
        auto entry_hash = combine(mix(entry.first), mix(entry.second));

        combined_hash ^= entry_hash;
        ++numElements;
    }

    return combine(combined_hash, numElements);
}

// map of maps
template <typename MM>
[[nodiscard]] constexpr auto mapmap(const MM& mapmap) -> uint64_t {
    uint64_t combined_hash = 1;

    uint64_t numElements = 0;
    for (auto const& entry : mapmap) {
        auto entry_hash = combine(mix(entry.first), map(entry.second));

        combined_hash ^= entry_hash;
        ++numElements;
    }

    return combine(combined_hash, numElements);
}

} // namespace checksum
