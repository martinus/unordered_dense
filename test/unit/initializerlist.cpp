#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <string>

TEST_CASE("initializerlist_string") {
    size_t const n1 = 17;
    size_t const n2 = 10;

    auto m1 = ankerl::unordered_dense::map<std::string, size_t>{{"duck", n1}, {"lion", n2}};
    auto m2 = ankerl::unordered_dense::map<std::string, size_t>{{"duck", n1}, {"lion", n2}};

    REQUIRE(m1.size() == 2);
    REQUIRE(m1["duck"] == n1);
    REQUIRE(m1["lion"] == n2);

    REQUIRE(m2.size() == 2);
    auto it = m2.find("duck");
    REQUIRE((it != m2.end() && it->second == n1));
    REQUIRE(m2["lion"] == n2);
}
