#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// visit() looks up a range of keys in one go. What is testable about it is not the pipelining --
// that is invisible by construction -- but that it answers exactly what a loop of find() would,
// including at the edges the three-pass chunking creates: ranges shorter than a chunk, ranges that
// are an exact multiple of one, keys that repeat, keys that miss, and keys that do not stop in
// their home group, which is the path the chunk does not pipeline.

namespace {
// A hash the test chooses, so that keys can be steered into one group and made to overflow it --
// the same construction fuzz_group_index uses.
struct steered_hash {
    using is_avalanching = void;
    auto operator()(std::uint64_t k) const noexcept -> std::uint64_t {
        return k;
    }
};
auto steered(std::uint8_t group, std::uint8_t id, std::uint8_t fingerprint) -> std::uint64_t {
    return (static_cast<std::uint64_t>(group) << 56U) | (static_cast<std::uint64_t>(id) << 8U) | fingerprint;
}
} // namespace

TEST_CASE("visit_agrees_with_find_over_every_range_length") {
    auto map = ankerl::unordered_dense::map<uint64_t, size_t>();
    auto present = std::vector<uint64_t>();
    for (size_t i = 0; i < 4000; ++i) {
        auto const k = static_cast<uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15);
        map[k] = i;
        present.push_back(k);
    }
    // Interleaved with keys that really are absent. Setting the top bit is not enough to make one:
    // a scrambled key already has it set about half the time, so `k | 1<<63` is often k itself --
    // which the first version of this test did, and then found six thousand of four thousand.
    auto keys = std::vector<uint64_t>();
    auto absent = uint64_t{1};
    for (auto k : present) {
        keys.push_back(k);
        while (map.count(absent) != 0) {
            ++absent;
        }
        keys.push_back(absent++);
    }

    // Every length from empty to past two chunks, so the last partial chunk and the exact-multiple
    // case are both covered, and then the whole thing.
    for (size_t len = 0; len <= 40; ++len) {
        auto sum = size_t{0};
        auto const n = map.visit(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(len), [&](auto const& kv) {
            sum += kv.second;
        });
        auto expect_n = size_t{0};
        auto expect_sum = size_t{0};
        for (size_t i = 0; i < len; ++i) {
            if (auto it = map.find(keys[i]); it != map.end()) {
                ++expect_n;
                expect_sum += it->second;
            }
        }
        REQUIRE(n == expect_n);
        REQUIRE(sum == expect_sum);
    }

    auto total = size_t{0};
    auto const found = map.visit(keys.begin(), keys.end(), [&](auto const& kv) { total += kv.second; });
    REQUIRE(found == 4000U);
    REQUIRE(total == 4000U * 3999U / 2U);
}

// An empty map has no bucket array at all, which the first pass would otherwise index.
TEST_CASE("visit_on_an_empty_map") {
    auto map = ankerl::unordered_dense::map<std::string, int>();
    auto keys = std::vector<std::string>{"a", "b", "c"};
    auto calls = 0;
    REQUIRE(map.visit(keys.begin(), keys.end(), [&](auto const&) { ++calls; }) == 0U);
    REQUIRE(calls == 0);
    REQUIRE(map.visit(keys.begin(), keys.begin(), [&](auto const&) { ++calls; }) == 0U);
    REQUIRE(calls == 0);
}

// The per-key fallback: a key that does not stop in its home group is the ~5% the chunk does not
// pipeline, and it is reached through the same out-of-line walk a single lookup uses. Steered on
// purpose rather than hoped for, because a wrong answer there is silent.
TEST_CASE("visit_finds_keys_that_are_not_in_their_home_group") {
    auto map = ankerl::unordered_dense::map<uint64_t, size_t, steered_hash>();
    map.reserve(17);
    auto keys = std::vector<uint64_t>();
    for (std::uint8_t i = 0; i < 16U; ++i) {
        auto const k = steered(0, i, static_cast<std::uint8_t>(0x10U + i));
        map[k] = i;
        keys.push_back(k);
    }
    // group 0 is full, so this one lands past it
    auto const overflowed = steered(0, 200, 0x03U);
    map[overflowed] = 999;
    keys.push_back(overflowed);
    keys.push_back(steered(0, 201, 0x03U)); // same class, never inserted

    auto sum = size_t{0};
    auto const found = map.visit(keys.begin(), keys.end(), [&](auto const& kv) { sum += kv.second; });
    REQUIRE(found == 17U);
    REQUIRE(sum == 999U + 15U * 16U / 2U);
}

TEST_CASE("visit_repeated_keys_are_visited_each_time") {
    auto map = ankerl::unordered_dense::map<int, int>();
    map[1] = 10;
    map[2] = 20;
    auto keys = std::vector<int>(40, 1); // one key, forty times, spanning chunks
    auto calls = 0;
    auto sum = 0;
    REQUIRE(map.visit(keys.begin(), keys.end(), [&](auto const& kv) { ++calls; sum += kv.second; }) == 40U);
    REQUIRE(calls == 40);
    REQUIRE(sum == 400);
}

TEST_CASE("visit_can_modify_through_a_mutable_map") {
    auto map = ankerl::unordered_dense::map<int, int>();
    for (int i = 0; i < 100; ++i) {
        map[i] = i;
    }
    auto keys = std::vector<int>();
    for (int i = 0; i < 100; i += 2) {
        keys.push_back(i);
    }
    REQUIRE(map.visit(keys.begin(), keys.end(), [](auto& kv) { kv.second = -kv.second; }) == 50U);
    REQUIRE(map[10] == -10);
    REQUIRE(map[11] == 11);
}

TEST_CASE("visit_on_a_const_map_hands_out_const_references") {
    auto map = ankerl::unordered_dense::map<int, int>();
    map[1] = 10;
    auto const& cmap = map;
    auto keys = std::vector<int>{1, 2};
    auto sum = 0;
    auto const found = cmap.visit(keys.begin(), keys.end(), [&](auto const& kv) {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(kv)>>, "a const map visits const values");
        sum += kv.second;
    });
    REQUIRE(found == 1U);
    REQUIRE(sum == 10);
}

TEST_CASE("visit_on_a_set") {
    auto set = ankerl::unordered_dense::set<std::string>();
    set.insert("alpha");
    set.insert("beta");
    auto keys = std::vector<std::string>{"alpha", "gamma", "beta"};
    auto seen = std::vector<std::string>();
    REQUIRE(set.visit(keys.begin(), keys.end(), [&](auto const& k) { seen.push_back(k); }) == 2U);
    REQUIRE(seen == std::vector<std::string>{"alpha", "beta"});
}
