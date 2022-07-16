#include <ankerl/unordered_dense.h>

#define ENABLE_LOG_LINE
#include <app/print.h>

#include <doctest.h>

#include <type_traits> // for remove_reference, remove_referen...
#include <utility>     // for move
#include <vector>      // for vector

TEST_CASE("assign_to_moved") {
    auto a = ankerl::unordered_dense::map<int, int>();
    a[1] = 2;
    auto moved = std::move(a);
    REQUIRE(moved.size() == 1U);

    auto c = ankerl::unordered_dense::map<int, int>();
    c[3] = 4;

    // assign to a moved map
    a = c;
}

TEST_CASE("move_to_moved") {
    auto a = ankerl::unordered_dense::map<int, int>();
    a[1] = 2;
    auto moved = std::move(a);

    auto c = ankerl::unordered_dense::map<int, int>();
    c[3] = 4;

    // assign to a moved map
    a = std::move(c);

    a[5] = 6;
    moved[6] = 7;
    REQUIRE(moved[6] == 7);
}

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
