#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

// Which shape of probe a key type gets, pinned.
//
// A key whose comparison is a call makes the probe build a frame and spill the loop state before
// the first group is compared, because everything the loop keeps live has to survive that call.
// Handing everything past the home group to an out-of-line function removes the frame from the
// common path: measured 2026-09-10, a string miss goes from 140.6 instructions to 128.8 under
// clang and 118.0 to 113.2 under gcc. Doing the same where the comparison is a register compare
// costs an integer lookup 20% and a `pair<uint64_t, uint64_t>` one 40%.
//
// So the trait below decides a 40% question in both directions, and nothing else in the suite
// would notice it flipping -- every one of these keys keeps working either way, just slower. That
// is what these assertions are for.
namespace {

struct pod_key {
    std::uint64_t a{};
    std::uint64_t b{};
    auto operator==(pod_key const& o) const -> bool {
        return a == o.a && b == o.b;
    }
};

struct owning_key {
    std::string s{};
    auto operator==(owning_key const& o) const -> bool {
        return s == o.s;
    }
};

using ankerl::unordered_dense::detail::key_compare_is_call_v;

// Compared in registers: the probe stays one loop.
static_assert(!key_compare_is_call_v<std::uint64_t>);
static_assert(!key_compare_is_call_v<int>);
static_assert(!key_compare_is_call_v<pod_key>);
// `std::pair` and `std::tuple` write their own copy *assignment*, so they are not trivially
// copyable even when their members are. Keying the trait on `is_trivially_copyable` rather than on
// `is_trivially_copy_constructible` split them and cost 40%; this is that bug, pinned.
static_assert(!key_compare_is_call_v<std::pair<std::uint64_t, std::uint64_t>>);
static_assert(!key_compare_is_call_v<std::tuple<int, int>>);

// Compared through a call: the probe splits.
static_assert(key_compare_is_call_v<std::string>);
static_assert(key_compare_is_call_v<owning_key>);
static_assert(key_compare_is_call_v<std::pair<int, std::string>>);
// Trivially copyable, and still a call, which is why it is named rather than deduced.
static_assert(key_compare_is_call_v<std::string_view>);
static_assert(key_compare_is_call_v<std::wstring_view>);

} // namespace

// The split path, exercised where it actually differs: keys that do not stop in their home group.
// Both shapes have to agree with a reference map over inserts, lookups, misses and erases -- the
// out-of-line continuation carries the probe's termination test, so a mistake in it either loops
// forever or stops early, and stopping early is silent.
TEST_CASE("probe_split_agrees_with_a_reference_for_both_key_shapes") {
    auto strings = ankerl::unordered_dense::map<std::string, size_t>();
    auto integers = ankerl::unordered_dense::map<uint64_t, size_t>();
    auto ref = std::unordered_map<uint64_t, size_t>();

    // Enough entries at a high load that a good fraction of them sit past their home group.
    for (size_t i = 0; i < 20000; ++i) {
        auto const v = static_cast<uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15);
        strings[std::to_string(v)] = i;
        integers[v] = i;
        ref[v] = i;
    }
    REQUIRE(strings.size() == ref.size());
    REQUIRE(integers.size() == ref.size());

    for (auto const& [v, i] : ref) {
        REQUIRE(strings.at(std::to_string(v)) == i);
        REQUIRE(integers.at(v) == i);
    }
    // misses, which are what the counters and the continuation's bound decide
    for (size_t i = 0; i < 20000; ++i) {
        auto const v = (static_cast<uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15)) | (uint64_t{1} << 63U);
        REQUIRE(strings.count(std::to_string(v)) == ref.count(v));
        REQUIRE(integers.count(v) == ref.count(v));
    }
    // erase every second one, then look for all of them again: an erase decrements the counters the
    // continuation stops on.
    for (size_t i = 0; i < 20000; i += 2) {
        auto const v = static_cast<uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15);
        REQUIRE(strings.erase(std::to_string(v)) == 1U);
        REQUIRE(integers.erase(v) == 1U);
        ref.erase(v);
    }
    REQUIRE(strings.size() == ref.size());
    for (size_t i = 0; i < 20000; ++i) {
        auto const v = static_cast<uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15);
        REQUIRE(strings.count(std::to_string(v)) == ref.count(v));
        REQUIRE(integers.count(v) == ref.count(v));
    }
}

// A string_view key is trivially copyable and still splits, so it exercises the specialization
// rather than the default. Its bodies outlive the map, which is what makes the view valid.
TEST_CASE("probe_split_string_view_key") {
    auto bodies = std::vector<std::string>();
    bodies.reserve(5000);
    for (size_t i = 0; i < 5000; ++i) {
        bodies.push_back("key-" + std::to_string(i * 7919));
    }
    auto map = ankerl::unordered_dense::map<std::string_view, size_t>();
    for (size_t i = 0; i < bodies.size(); ++i) {
        map[bodies[i]] = i;
    }
    REQUIRE(map.size() == bodies.size());
    for (size_t i = 0; i < bodies.size(); ++i) {
        REQUIRE(map.at(bodies[i]) == i);
    }
    REQUIRE(map.count("not in it") == 0U);
}

// The split path's own early-out, steered rather than hoped for.
//
// `probe` now tests "did anything of this class overflow past my home group" in two places: inside
// the shared loop, and again in the split path's inline home-group check. A mutation sweep on
// 2026-09-10 turned the second one from `== 0` into `== 1` and **no test noticed** -- that stops a
// probe at home whenever exactly one entry of the key's class went past it, so the entry that went
// past becomes unfindable. Reaching it by chance needs a string whose home counter happens to be
// exactly one; reaching it on purpose needs a hash the test chooses, which is what
// fuzz_group_index does for the unsplit path and what this does for the split one.
//
// The key type is what selects the shape: a user-provided copy constructor makes it not trivially
// copy-constructible, so detail::key_compare_is_call is true and the probe splits.
namespace {

struct call_key {
    std::uint64_t v{};

    call_key() = default;
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions) -- terser test
    call_key(std::uint64_t x)
        : v(x) {}
    // user-provided on purpose: it is what makes this key take the split probe
    call_key(call_key const& o)
        : v(o.v) {}
    auto operator=(call_key const& o) -> call_key& {
        v = o.v;
        return *this;
    }
    ~call_key() = default;
    call_key(call_key&&) = default;
    auto operator=(call_key&&) -> call_key& = default;

    auto operator==(call_key const& o) const -> bool {
        return v == o.v;
    }
};

struct steered_call_hash {
    using is_avalanching = void;
    auto operator()(call_key const& k) const noexcept -> std::uint64_t {
        return k.v;
    }
};

static_assert(key_compare_is_call_v<call_key>, "this test is about the split probe; this key must take it");

// The top byte picks the group, the low byte the fingerprint, the middle keeps keys distinct --
// exactly fuzz_group_index's construction.
auto steered(std::uint8_t group, std::uint8_t id, std::uint8_t fingerprint) -> call_key {
    return call_key{(static_cast<std::uint64_t>(group) << 56U) | (static_cast<std::uint64_t>(id) << 8U) | fingerprint};
}

} // namespace

TEST_CASE("probe_split_home_group_counter_must_be_exact") {
    auto map = ankerl::unordered_dense::map<call_key, size_t, steered_call_hash>();
    map.reserve(17); // two groups, and 17 entries do not grow them
    auto const groups = map.bucket_count() / 16U;
    REQUIRE(map.bucket_count() >= 32U);

    // Sixteen keys that all call group 0 home, filling it exactly.
    for (std::uint8_t i = 0; i < 16U; ++i) {
        REQUIRE(map.try_emplace(steered(0, i, static_cast<std::uint8_t>(0x10U + i)), i).second);
    }
    REQUIRE(map.size() == 16U);

    // One more, of fingerprint class 3, which finds group 0 full: it lands in a later group and
    // leaves group 0's counter for class 3 at exactly one. That "one" is the number the mutation
    // stopped on.
    auto const overflowed = steered(0, 200, 0x03U);
    REQUIRE(map.try_emplace(overflowed, 999U).second);
    REQUIRE(map.size() == 17U);
    REQUIRE(map.bucket_count() / 16U == groups); // nothing grew, so the layout is still the one built above

    // The lookup that only succeeds if the home group's counter is compared against zero.
    REQUIRE(map.count(overflowed) == 1U);
    REQUIRE(map.find(overflowed) != map.end());
    REQUIRE(map.at(overflowed) == 999U);

    // Everything still there, and a miss of the same class still stops correctly.
    for (std::uint8_t i = 0; i < 16U; ++i) {
        REQUIRE(map.count(steered(0, i, static_cast<std::uint8_t>(0x10U + i))) == 1U);
    }
    REQUIRE(map.count(steered(0, 201, 0x03U)) == 0U);

    // Erasing the overflowed entry takes that counter back down; the fillers stay findable.
    REQUIRE(map.erase(overflowed) == 1U);
    REQUIRE(map.count(overflowed) == 0U);
    for (std::uint8_t i = 0; i < 16U; ++i) {
        REQUIRE(map.count(steered(0, i, static_cast<std::uint8_t>(0x10U + i))) == 1U);
    }
}
