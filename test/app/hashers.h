#pragma once

#include <ankerl/unordered_dense.h>

#include <cstdint>
#include <string_view>

// Hashers that several tests need and that carry no assertions of their own, so sharing them
// couples nothing. A `check_*` helper is a different matter and stays with its test.
namespace test {

// Transparent and avalanching: what a map needs before it will accept a lookup key that is not the
// key type, which is how the string_view overloads are reached.
struct transparent_hash {
    using is_transparent = void;
    using is_avalanching = void;

    [[nodiscard]] auto operator()(std::string_view sv) const noexcept -> std::uint64_t {
        return ankerl::unordered_dense::hash<std::string_view>{}(sv);
    }
};

// Avalanching and 32 bit -- the combination mixed_hash() multiplies up into the high bits, because
// that is where the bucket index is read from. Fibonacci hashing in 32 bits: genuinely avalanching,
// genuinely too narrow.
struct narrow_avalanching_hash {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(int x) const noexcept -> std::uint32_t {
        return static_cast<std::uint32_t>(x) * UINT32_C(2654435761);
    }
};

} // namespace test
