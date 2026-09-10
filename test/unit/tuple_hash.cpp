#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <third-party/nanobench.h> // for Rng, doNotOptimizeAway, Bench

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

TEST_CASE("tuple_hash") {
    auto m = ankerl::unordered_dense::map<std::pair<int, std::string>, int>();
    auto pair_hash = ankerl::unordered_dense::hash<std::pair<int, std::string>>{};
    REQUIRE(pair_hash(std::pair<int, std::string>{1, "a"}) != pair_hash(std::pair<int, std::string>{1, "b"}));

    m.try_emplace({1, "a"}, 23);
    m.try_emplace({1, "b"}, 42);
    REQUIRE(m.size() == 2U);
}

// What hashing a tuple actually computes, pinned.
//
// Every other case in this file asks whether *different* tuples hash differently, and they do
// whatever `to64` does with each element -- so the one decision in it is invisible to all of them.
// That decision is `if constexpr (is_integral_v<Arg> || is_enum_v<Arg>)`: an integral or an enum is
// cast straight to 64 bits and the map's own mixing does the rest, anything else is hashed first.
// Turning that `||` into `&&` sends every integer through `hash<Arg>{}` instead, changing the hash
// of every tuple in every program -- and a mutation sweep on 2026-09-10 found nothing noticed.
//
// As with hash_golden.cpp this is not a promise the values never change. It is a promise that
// changing them is a decision. The single-element rows are the sharpest: a one element tuple of an
// integer is that integer, unmixed, which is exactly the property the `||` branch exists for.
TEST_CASE("tuple_hash_golden_values") {
    enum class colour : std::uint8_t { red = 1, green = 2 };
    using ankerl::unordered_dense::hash;

    REQUIRE(hash<std::tuple<>>{}({}) == UINT64_C(0x0000000000000000));
    REQUIRE(hash<std::tuple<std::uint64_t>>{}({UINT64_MAX}) == UINT64_C(0xffffffffffffffff));
    REQUIRE(hash<std::pair<int, int>>{}({1, 2}) == UINT64_C(0x0008a6bc006f6e06));
    REQUIRE(hash<std::pair<int, int>>{}({2, 1}) == UINT64_C(0xa2312f5f36a55294));
    REQUIRE(hash<std::tuple<std::uint8_t, std::uint8_t, std::uint8_t>>{}({1, 2, 3}) == UINT64_C(0xd34d2e086bbe0e77));
    REQUIRE(hash<std::tuple<int, std::string>>{}({7, "hello"}) == UINT64_C(0x1140b91276c83448));
    REQUIRE(hash<std::tuple<colour, int>>{}({colour::green, 5}) == UINT64_C(0xdab19772c384ecfa));
}

TEST_CASE("good_tuple_hash") {
    auto hashes = ankerl::unordered_dense::set<uint64_t>();

    auto t = std::tuple<uint8_t, uint8_t, uint8_t>();
    for (size_t i = 0; i < 256 * 256; ++i) {
        std::get<0>(t) = static_cast<uint8_t>(i);
        std::get<2>(t) = static_cast<uint8_t>(i / 256);
        hashes.emplace(ankerl::unordered_dense::hash<decltype(t)>{}(t));
    }

    REQUIRE(hashes.size() == 256 * 256);
}

TEST_CASE("tuple_hash_with_stringview") {
    using T = std::tuple<int, std::string_view>;

    auto t = T();
    std::get<0>(t) = 1;
    auto str = std::string("hello");
    std::get<1>(t) = str;

    auto h1 = ankerl::unordered_dense::hash<T>{}(t);
    str = "world";
    REQUIRE(std::get<1>(t) == std::string{"world"});
    auto h2 = ankerl::unordered_dense::hash<T>{}(t);
    REQUIRE(h1 != h2);
}

// #include <absl/hash/hash.h>

TEST_CASE("bench_tuple_hash" * doctest::test_suite("bench") * doctest::skip()) {
    using T = std::tuple<uint8_t, int, uint16_t, uint64_t>;

    auto vecs = std::vector<T>(100);
    auto rng = ankerl::nanobench::Rng(123);
    for (auto& v : vecs) {
        std::get<0>(v) = static_cast<uint8_t>(rng());
        std::get<1>(v) = static_cast<int>(rng());
        std::get<2>(v) = static_cast<uint16_t>(rng());
        std::get<3>(v) = static_cast<uint64_t>(rng());
    }

    uint64_t h = 0;
    ankerl::nanobench::Bench().batch(vecs.size()).run("ankerl hash", [&] {
        for (auto const& v : vecs) {
            h += ankerl::unordered_dense::hash<T>{}(v);
            // h += absl::Hash<T>{}(v);
        }
    });
    ankerl::nanobench::doNotOptimizeAway(h);
}
