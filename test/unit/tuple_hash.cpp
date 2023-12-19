#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <third-party/nanobench.h> // for Rng, doNotOptimizeAway, Bench

#include <string>
#include <string_view>

TEST_CASE("tuple_hash") {
    auto m = ankerl::unordered_dense::map<std::pair<int, std::string>, int>();
    auto pair_hash = ankerl::unordered_dense::hash<std::pair<int, std::string>>{};
    REQUIRE(pair_hash(std::pair<int, std::string>{1, "a"}) != pair_hash(std::pair<int, std::string>{1, "b"}));

    m.try_emplace({1, "a"}, 23);
    m.try_emplace({1, "b"}, 42);
    REQUIRE(m.size() == 2U);
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

TEST_CASE("bench_tuple_hash" * doctest::test_suite("bench")) {
    using T = std::tuple<char, int, uint16_t, std::byte, uint64_t>;

    auto h = uint64_t{};
    auto t = std::tuple<char, int, uint16_t, std::byte, uint64_t>{};
    ankerl::nanobench::Bench().run("ankerl hash", [&] {
        h += ankerl::unordered_dense::hash<T>{}(t);
        ++std::get<4>(t);
    });
}
