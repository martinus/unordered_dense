#include <ankerl/unordered_dense.h>

#define ENABLE_LOG_LINE
#include <app/print.h>

#include <doctest.h>

#include <vector>

TEST_CASE("swap") {
    {
        auto b = ankerl::unordered_dense::map<int, int>();
        {
            auto a = ankerl::unordered_dense::map<int, int>();
            b[1] = 2;

            a.swap(b);
            REQUIRE(a.end() != a.find(1));
            REQUIRE(b.end() == b.find(1));
        }
        REQUIRE(b.end() == b.find(1));
        b[2] = 3;
        REQUIRE(b.end() != b.find(2));
        REQUIRE(b.size() == 1);
    }

    {
        auto a = ankerl::unordered_dense::map<int, int>();
        {
            auto b = ankerl::unordered_dense::map<int, int>();
            b[1] = 2;

            a.swap(b);
            REQUIRE(a.end() != a.find(1));
            REQUIRE(b.end() == b.find(1));
        }
        REQUIRE(a.end() != a.find(1));
        a[2] = 3;
        REQUIRE(a.end() != a.find(2));
        REQUIRE(a.size() == 2);
    }

    {
        auto a = ankerl::unordered_dense::map<int, int>();
        {
            auto b = ankerl::unordered_dense::map<int, int>();
            a.swap(b);
            REQUIRE(a.end() == a.find(1));
            REQUIRE(b.end() == b.find(1));
        }
        REQUIRE(a.end() == a.find(1));
    }
}
