#include <ankerl/unordered_dense.h>

#include <app/counter.h>
#include <app/doctest.h>

#include <string>  // for string
#include <tuple>   // for forward_as_tuple
#include <utility> // for piecewise_construct
#include <vector>  // for vector

TEST_CASE_MAP("insert", unsigned int, int) {
    auto map = map_t();
    auto const val = typename map_t::value_type(123U, 321);
    map.insert(val);
    REQUIRE(map.size() == 1);

    REQUIRE(map[123U] == 321);
}

TEST_CASE_MAP("insert_hint", unsigned int, int) {
    auto map = map_t();
    auto it = map.insert(map.begin(), {1, 2});

    auto vt = typename decltype(map)::value_type{3, 4};
    map.insert(it, vt);
    REQUIRE(map.size() == 2);
    REQUIRE(map[1] == 2);
    REQUIRE(map[3] == 4);

    auto const vt2 = typename decltype(map)::value_type{10, 11};
    map.insert(it, vt2);
    REQUIRE(map.size() == 3);
    REQUIRE(map[10] == 11);

    it = map.emplace_hint(it, std::piecewise_construct, std::forward_as_tuple(123), std::forward_as_tuple(321));
    REQUIRE(map.size() == 4);
    REQUIRE(map[123] == 321);
}

// insert() and emplace() look the key up before constructing anything when the arguments carry it
// (a whole value, or a map's key and mapped value), so a key that is present costs no copy, and a
// value handed over as an rvalue is not moved from.
TEST_CASE_MAP("insert_present_key_leaves_the_argument", std::string, std::string) {
    auto map = map_t();
    map.try_emplace("key", "first");
    auto vt = typename map_t::value_type("key", std::string(100, 'x'));
    auto const r = map.insert(std::move(vt));
    REQUIRE_FALSE(r.second);
    REQUIRE(r.first->second == "first");
    REQUIRE(map.size() == 1);
    REQUIRE(vt.second == std::string(100, 'x')); // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
}

TEST_CASE_SET("insert_present_key_leaves_the_argument_set", std::string) {
    auto set = set_t();
    set.insert(std::string(100, 'x'));
    auto key = std::string(100, 'x');
    REQUIRE_FALSE(set.insert(std::move(key)).second);
    REQUIRE(set.size() == 1);
    REQUIRE(key == std::string(100, 'x')); // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
}

TEST_CASE_MAP("insert_present_key_copies_nothing", counter::obj, counter::obj) {
    auto counts = counter();
    INFO(counts);
    auto map = map_t();
    auto const vt = typename map_t::value_type({1, counts}, {2, counts});
    map.insert(vt);
    auto const before = counts.data();
    REQUIRE_FALSE(map.insert(vt).second);
    REQUIRE(counts.copy_ctor() == before.m_copy_ctor);
    REQUIRE(counts.dtor() == before.m_dtor);
}

TEST_CASE_MAP("emplace_key_and_mapped_present_key_leaves_the_arguments", std::string, std::string) {
    auto map = map_t();
    map.try_emplace("key", "first");
    auto key = std::string("key");
    auto mapped = std::string(100, 'x');
    REQUIRE_FALSE(map.emplace(key, std::move(mapped)).second);
    REQUIRE(map.size() == 1);
    REQUIRE(map["key"] == "first");
    REQUIRE(mapped == std::string(100, 'x')); // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
}

namespace {

// A mapped value built from something else, which counts how often that happens.
struct converted {
    static inline int constructed = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    int m_value;
    explicit converted(int v)
        : m_value(v) {
        ++constructed;
    }
};

} // namespace

// Arguments the value is converted from are still consumed on a present key -- the value is built
// and destroyed, as emplace always did -- so that emplace(k, new int) cannot leak its pointer.
TEST_CASE_MAP("emplace_converting_arguments_are_consumed_on_a_present_key", int, converted) {
    auto map = map_t();
    map.emplace(1, 10);
    auto const before = converted::constructed;
    REQUIRE_FALSE(map.emplace(1, 20).second);
    REQUIRE(converted::constructed == before + 1);
    REQUIRE(map.find(1)->second.m_value == 10);
}

// emplace() without arguments has no key to look up first: it builds a default value and inserts
// that, once.
TEST_CASE_MAP("emplace_without_arguments", int, int) {
    auto map = map_t();
    REQUIRE(map.emplace().second);
    REQUIRE_FALSE(map.emplace().second);
    REQUIRE(map.size() == 1);
    REQUIRE(map.find(0)->second == 0);
}

TEST_CASE_SET("emplace_without_arguments_set", int) {
    auto set = set_t();
    REQUIRE(set.emplace().second);
    REQUIRE_FALSE(set.emplace().second);
    REQUIRE(set.size() == 1);
}
