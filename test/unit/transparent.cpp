#include <ankerl/unordered_dense.h>

#include <app/name_of_type.h>

#include <doctest.h>
#include <fmt/format.h>

#include <array>         // for array
#include <cstddef>       // for size_t
#include <functional>    // for equal_to
#include <string>        // for basic_string, string, operator""s
#include <string_view>   // for basic_string_view, operator""sv
#include <type_traits>   // for add_const_t
#include <unordered_map> // for unordered_map
#include <utility>       // for pair, as_const
#include <vector>        // for vector

using namespace std::literals;

class string_eq {
    mutable std::unordered_map<std::string, size_t> m_names_to_counts{};

public:
    using is_transparent = void;

    template <class T, class U>
    auto operator()(T&& lhs, U&& rhs) const -> decltype(std::forward<T>(lhs) == std::forward<U>(rhs)) {
        auto names = fmt::format("{} -> {}", name_of_type<T>(), name_of_type<U>());
        ++m_names_to_counts[names];
        return std::forward<T>(lhs) == std::forward<U>(rhs);
    }

    auto counts() const -> std::unordered_map<std::string, size_t> const& {
        return m_names_to_counts;
    }

    string_eq() = default;
    ~string_eq() = default;

    string_eq(string_eq const&) = default;
    auto operator=(string_eq const&) -> string_eq& = default;

    string_eq(string_eq&&) = delete;
    auto operator=(string_eq&&) -> string_eq& = delete;
};

// transparent hash, counts number of calls per operator
class string_hash {
    mutable size_t m_num_charstar{};
    mutable size_t m_num_stringview{};
    mutable size_t m_num_string{};

public:
    using hash_type = ankerl::unordered_dense::hash<std::string_view>;
    using is_transparent = void;
    using is_avalanching = void;

    [[nodiscard]] auto operator()(const char* str) const noexcept -> uint64_t {
        ++m_num_charstar;
        return hash_type{}(str);
    }

    [[nodiscard]] auto operator()(std::string_view str) const noexcept -> uint64_t {
        ++m_num_stringview;
        return hash_type{}(str);
    }

    [[nodiscard]] auto operator()(std::string const& str) const noexcept -> uint64_t {
        ++m_num_string;
        return hash_type{}(str);
    }

    auto counts() const -> std::array<size_t, 3> {
        return {m_num_charstar, m_num_stringview, m_num_string};
    }
};

namespace docs {

struct string_hash {
    using is_transparent = void; // enable heterogenous lookup
    using is_avalanching = void; // mark class as high quality avalanching hash

    [[nodiscard]] auto operator()(const char* str) const noexcept -> uint64_t {
        return ankerl::unordered_dense::hash<std::string_view>{}(str);
    }

    [[nodiscard]] auto operator()(std::string_view str) const noexcept -> uint64_t {
        return ankerl::unordered_dense::hash<std::string_view>{}(str);
    }

    [[nodiscard]] auto operator()(std::string const& str) const noexcept -> uint64_t {
        return ankerl::unordered_dense::hash<std::string_view>{}(str);
    }
};

} // namespace docs

void check(string_hash const& sh, size_t num_charstar, size_t num_stringview, size_t num_string) {
    auto counts = sh.counts();
    INFO(counts[0] << " charstar, " << counts[1] << " string_view, " << counts[2] << " string");
    REQUIRE(counts[0] == num_charstar);
    REQUIRE(counts[1] == num_stringview);
    REQUIRE(counts[2] == num_string);
}

TEST_CASE("transparent_find") {
    auto map = ankerl::unordered_dense::map<std::string, size_t, string_hash, std::equal_to<>>();
    map.try_emplace("hello", 1);
    check(map.hash_function(), 0, 0, 1);

    auto it = map.find("huh");
    check(map.hash_function(), 1, 0, 1);
    REQUIRE(it == map.end());
    it = map.find("hello");
    check(map.hash_function(), 2, 0, 1);
    REQUIRE(it != map.end());

    auto cit = std::as_const(map).find("huh");
    check(map.hash_function(), 3, 0, 1);
    REQUIRE(cit == map.end());
    REQUIRE(cit == map.cend());
    cit = std::as_const(map).find("hello");
    check(map.hash_function(), 4, 0, 1);
    REQUIRE(cit != map.end());

    // string_view
    it = map.find("huh"sv);
    REQUIRE(it == map.end());
    check(map.hash_function(), 4, 1, 1);
    it = map.find("hello"sv);
    REQUIRE(it != map.end());
    check(map.hash_function(), 4, 2, 1);

    // string
    it = map.find("huh"s);
    REQUIRE(it == map.end());
    check(map.hash_function(), 4, 2, 2);
    it = map.find("hello"s);
    REQUIRE(it != map.end());
    check(map.hash_function(), 4, 2, 3);
}

TEST_CASE("transparent_count") {
    auto map = ankerl::unordered_dense::map<std::string, size_t, string_hash, std::equal_to<>>();
    map.try_emplace("hello", 1);
    check(map.hash_function(), 0, 0, 1);

    REQUIRE(0 == map.count("huh"));
    check(map.hash_function(), 1, 0, 1);
    REQUIRE(1 == map.count("hello"));
    check(map.hash_function(), 2, 0, 1);

    REQUIRE(0 == map.count("huh"sv));
    check(map.hash_function(), 2, 1, 1);
    REQUIRE(1 == map.count("hello"sv));
    check(map.hash_function(), 2, 2, 1);

    REQUIRE(0 == map.count("huh"s));
    check(map.hash_function(), 2, 2, 2);
    REQUIRE(1 == map.count("hello"s));
    check(map.hash_function(), 2, 2, 3);
}

TEST_CASE("transparent_contains") {
    auto map = ankerl::unordered_dense::map<std::string, size_t, string_hash, std::equal_to<>>();
    map.try_emplace("hello", 1);
    check(map.hash_function(), 0, 0, 1);

    REQUIRE(!map.contains("huh"));
    check(map.hash_function(), 1, 0, 1);
    REQUIRE(map.contains("hello"));
    check(map.hash_function(), 2, 0, 1);

    REQUIRE(!map.contains("huh"sv));
    check(map.hash_function(), 2, 1, 1);
    REQUIRE(map.contains("hello"sv));
    check(map.hash_function(), 2, 2, 1);

    REQUIRE(!map.contains("huh"s));
    check(map.hash_function(), 2, 2, 2);
    REQUIRE(map.contains("hello"s));
    check(map.hash_function(), 2, 2, 3);
}

TEST_CASE("transparent_erase") {
    auto map = ankerl::unordered_dense::map<std::string, size_t, string_hash, std::equal_to<>>();
    map.try_emplace("hello", 1);
    check(map.hash_function(), 0, 0, 1);
    REQUIRE(0 == map.erase("huh"));
    check(map.hash_function(), 1, 0, 1);
    REQUIRE(1 == map.erase("hello"));
    check(map.hash_function(), 2, 0, 1);

    map.try_emplace("hello", 1);
    check(map.hash_function(), 2, 0, 2);
    REQUIRE(0 == map.erase("huh"sv));
    check(map.hash_function(), 2, 1, 2);
    REQUIRE(1 == map.erase("hello"sv));
    check(map.hash_function(), 2, 2, 2);

    map.try_emplace("hello", 1);
    check(map.hash_function(), 2, 2, 3);
    REQUIRE(0 == map.erase("huh"s));
    check(map.hash_function(), 2, 2, 4);
    REQUIRE(1 == map.erase("hello"s));
    check(map.hash_function(), 2, 2, 5);
}

TEST_CASE("transparent_equal_range") {
    auto map = ankerl::unordered_dense::map<std::string, size_t, string_hash, std::equal_to<>>();
    map.try_emplace("hello", 1);
    check(map.hash_function(), 0, 0, 1);

    auto range = map.equal_range("hello");
    check(map.hash_function(), 1, 0, 1);
    REQUIRE(range.first != range.second);
    REQUIRE(range.first->first == "hello");
    REQUIRE(range.second == map.end());

    auto crange = std::as_const(map).equal_range("hello"sv);
    check(map.hash_function(), 1, 1, 1);
    REQUIRE(crange.first != range.second);
    REQUIRE(crange.first->first == "hello");
    REQUIRE(crange.second == map.end());
}

TEST_CASE("transparent_string_eq") {
    auto map = ankerl::unordered_dense::map<std::string, size_t, string_hash, string_eq>();
    map.try_emplace("hello", 1);

    REQUIRE(map.count("hello"));
    REQUIRE(map.count("hello"sv));
    REQUIRE(map.count("hello"s));

    auto ke = map.key_eq();
    REQUIRE(ke.counts().size() == 3);
    for (auto const& kv : ke.counts()) {
        REQUIRE(kv.second == 1U);
    }
}
