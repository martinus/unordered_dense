#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <cstddef> // for size_t
#include <string>  // for allocator, string, operator==
#include <utility> // for pair, move
#include <vector>  // for vector

struct Foo {
    uint64_t i;
};

template <>
struct std::hash<Foo> {
    auto operator()(Foo const& foo) const noexcept {
        return static_cast<size_t>(foo.i + 1);
    }
};

TEST_CASE("std_hash") {
    auto f = Foo{12345};
    REQUIRE(std::hash<Foo>{}(f) == 12346U);
    // unordered_dense::hash blows that up to 64bit!
    REQUIRE(ankerl::unordered_dense::hash<Foo>{}(f) == UINT64_C(0x3F645BE4CE24110C));
    REQUIRE(ankerl::unordered_dense::hash<uint64_t>{}(12346U) == UINT64_C(0x3F645BE4CE24110C));
}
