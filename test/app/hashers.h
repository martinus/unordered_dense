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

// A hash and an equality that carry a tag, so that "which instance did this table end up with"
// is a question a test can ask at all. std::hash and std::equal_to are stateless, so a table that
// swaps its elements and forgets its hasher answers every question about its contents correctly
// and is still wrong.
//
// The tag deliberately takes no part in the hashing or the comparison. Two tables carrying
// different tags therefore agree about every key, which is what makes the tag able to answer only
// the question it is here for -- and what makes it safe to hand a table the "wrong" one.
struct tagged_hash {
    int tag = 0;

    [[nodiscard]] auto operator()(int x) const noexcept -> std::uint64_t {
        return ankerl::unordered_dense::hash<int>{}(x);
    }
};

struct tagged_equal {
    int tag = 0;

    [[nodiscard]] auto operator()(int a, int b) const noexcept -> bool {
        return a == b;
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
