#include <ankerl/unordered_dense.h>

#define ENABLE_LOG_LINE
#include <app/print.h>

#include <doctest.h>

#include <cstdint> // for uint64_t
#include <utility> // for pair
#include <vector>  // for vector

using Map = ankerl::unordered_dense::map<uint64_t, uint64_t>;

TEST_CASE("assignment_combinations_1") {
    Map a;
    Map b;
    b = a;
    REQUIRE(b == a);
}

TEST_CASE("assignment_combinations_2") {
    Map a;
    Map const& aConst = a;
    Map b;
    a[123] = 321;
    b = a;

    REQUIRE(a.find(123)->second == 321);
    REQUIRE(aConst.find(123)->second == 321);

    REQUIRE(b.find(123)->second == 321);
    a[123] = 111;
    REQUIRE(a.find(123)->second == 111);
    REQUIRE(aConst.find(123)->second == 111);
    REQUIRE(b.find(123)->second == 321);
    b[123] = 222;
    REQUIRE(a.find(123)->second == 111);
    REQUIRE(aConst.find(123)->second == 111);
    REQUIRE(b.find(123)->second == 222);
}

TEST_CASE("assignment_combinations_3") {
    Map a;
    Map b;
    a[123] = 321;
    a.clear();
    b = a;

    REQUIRE(a.size() == 0);
    REQUIRE(b.size() == 0);
}

TEST_CASE("assignment_combinations_4") {
    Map a;
    Map b;
    b[123] = 321;
    b = a;

    REQUIRE(a.size() == 0);
    REQUIRE(b.size() == 0);
}

TEST_CASE("assignment_combinations_5") {
    Map a;
    Map b;
    b[123] = 321;
    b.clear();
    b = a;

    REQUIRE(a.size() == 0);
    REQUIRE(b.size() == 0);
}

TEST_CASE("assignment_combinations_6") {
    Map a;
    a[1] = 2;
    Map b;
    b[3] = 4;
    b = a;

    REQUIRE(a.size() == 1);
    REQUIRE(b.size() == 1);
    REQUIRE(b.find(1)->second == 2);
    a[1] = 123;
    REQUIRE(a.size() == 1);
    REQUIRE(b.size() == 1);
    REQUIRE(b.find(1)->second == 2);
}

TEST_CASE("assignment_combinations_7") {
    Map a;
    a[1] = 2;
    a.clear();
    Map b;
    REQUIRE(a == b);
    b[3] = 4;
    REQUIRE(a != b);
    b = a;
    REQUIRE(a == b);
}

TEST_CASE("assignment_combinations_7") {
    Map a;
    a[1] = 2;
    Map b;
    REQUIRE(a != b);
    b[3] = 4;
    b.clear();
    REQUIRE(a != b);
    b = a;
    REQUIRE(a == b);
}

TEST_CASE("assignment_combinations_8") {
    Map a;
    a[1] = 2;
    a.clear();
    Map b;
    b[3] = 4;
    REQUIRE(a != b);
    b.clear();
    REQUIRE(a == b);
    b = a;
    REQUIRE(a == b);
}

TEST_CASE("assignment_combinations_9") {
    Map a;
    a[1] = 2;

    // self assignment should work too
    Map* b = &a;
    a = *b;
    REQUIRE(a == a);
    REQUIRE(a.size() == 1);
    REQUIRE(a.find(1) != a.end());
}

TEST_CASE("assignment_combinations_10") {
    Map a;
    a[1] = 2;
    Map b;
    b[2] = 1;

    // maps have the same number of elements, but are not equal.
    REQUIRE(!(a == b));
    REQUIRE(a != b);
    REQUIRE(b != a);
    REQUIRE(!(a == b));
    REQUIRE(!(b == a));

    Map c;
    c[1] = 3;
    REQUIRE(a != c);
    REQUIRE(c != a);
    REQUIRE(!(a == c));
    REQUIRE(!(c == a));

    b.clear();
    REQUIRE(a != b);
    REQUIRE(b != a);
    REQUIRE(!(a == b));
    REQUIRE(!(b == a));

    Map empty;
    REQUIRE(b == empty);
    REQUIRE(empty == b);
    REQUIRE(!(b != empty));
    REQUIRE(!(empty != b));
}
