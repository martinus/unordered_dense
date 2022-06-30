#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <string>

struct RegularType {
    // cppcheck-suppress passedByValue
    RegularType(std::size_t i_, std::string s_) noexcept
        : s(std::move(s_))
        , i(i_) {}

    friend auto operator==(const RegularType& r1, const RegularType& r2) -> bool {
        return r1.i == r2.i && r1.s == r2.s;
    }

private:
    std::string s;
    std::size_t i;
};

TEST_CASE("try_emplace") {
    using Map = ankerl::unordered_dense::map<std::string, RegularType>;
    Map map;
    auto ret = map.try_emplace("a", 1U, "b");
    REQUIRE(ret.second);
    REQUIRE(ret.first == map.find("a"));
    REQUIRE(ret.first->second == RegularType(1U, "b"));
    REQUIRE(map.size() == 1);

    ret = map.try_emplace("c", 2U, "d");
    REQUIRE(ret.second);
    REQUIRE(ret.first == map.find("c"));
    REQUIRE(ret.first->second == RegularType(2U, "d"));
    REQUIRE(map.size() == 2U);

    std::string key = "c";
    ret = map.try_emplace(key, 3U, "dd");
    REQUIRE_FALSE(ret.second);
    REQUIRE(ret.first == map.find("c"));
    REQUIRE(ret.first->second == RegularType(2U, "d"));
    REQUIRE(map.size() == 2U);

    key = "a";
    RegularType value(3U, "dd");
    ret = map.try_emplace(key, value);
    REQUIRE_FALSE(ret.second);
    REQUIRE(ret.first == map.find("a"));
    REQUIRE(ret.first->second == RegularType(1U, "b"));
    REQUIRE(map.size() == 2U);

    auto pos = map.try_emplace(map.end(), "e", 67U, "f");
    REQUIRE(pos == map.find("e"));
    REQUIRE(pos->second == RegularType(67U, "f"));
    REQUIRE(map.size() == 3U);

    key = "e";
    RegularType value2(66U, "ff");
    pos = map.try_emplace(map.begin(), key, value2);
    REQUIRE(pos == map.find("e"));
    REQUIRE(pos->second == RegularType(67U, "f"));
    REQUIRE(map.size() == 3U);
}
