#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <string>

TEST_CASE("copyassignment") {
    auto map = ankerl::unordered_dense::map<std::string, std::string>();
    auto tmp = ankerl::unordered_dense::map<std::string, std::string>();

    map.emplace("a", "b");
    map = tmp;
    map.emplace("c", "d");

    REQUIRE(map.size() == 1);
    REQUIRE(map["c"] == "d");
    REQUIRE(map.size() == 1);

    REQUIRE(tmp.size() == 0);

    map["e"] = "f";
    REQUIRE(map.size() == 2);
    REQUIRE(tmp.size() == 0);

    tmp["g"] = "h";
    REQUIRE(map.size() == 2);
    REQUIRE(tmp.size() == 1);
}
