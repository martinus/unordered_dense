#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <string> // for operator==, basic_string, operat...
#include <vector> // for vector

TEST_CASE("set") {
    auto set = ankerl::unordered_dense::set<std::string>();

    set.insert("asdf");
    REQUIRE(set.size() == 1);
    auto it = set.find("asdf");
    REQUIRE(it != set.end());
    REQUIRE(*it == "asdf");
}
