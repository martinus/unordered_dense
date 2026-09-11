#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// prefetch() starts the memory access a later lookup will make and hands back the hash it had to
// compute. Nothing it does is observable by itself -- a prefetch has no result and a wrong address
// is merely a wasted one -- so what is testable is the hash it returns, that it agrees with
// hash_for, that it is safe on a table with no buckets, and that the overloads resolve the way they
// are meant to. The last of those is the one that could break silently.

TEST_CASE("prefetch_returns_the_same_hash_as_hash_for") {
    auto map = ankerl::unordered_dense::map<std::string, int>();
    map["alpha"] = 1;
    map["beta"] = 2;

    for (auto const& key : {std::string("alpha"), std::string("beta"), std::string("absent")}) {
        REQUIRE(map.prefetch(key).m_mixed_hash == map.hash_for(key).m_mixed_hash);
    }
}

// The point of the return value: a pipelined loop hashes each key once rather than twice.
TEST_CASE("prefetch_hash_drives_the_lookup_overloads") {
    auto map = ankerl::unordered_dense::map<uint64_t, size_t>();
    auto keys = std::vector<uint64_t>();
    for (size_t i = 0; i < 5000; ++i) {
        auto const k = static_cast<uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15);
        keys.push_back(k);
        map[k] = i;
    }

    constexpr size_t depth = 8;
    auto ring = std::array<ankerl::unordered_dense::map<uint64_t, size_t>::precomputed_hash, depth>{};
    for (size_t i = 0; i < depth && i < keys.size(); ++i) {
        ring[i] = map.prefetch(keys[i]);
    }
    auto sum = size_t{0};
    for (size_t i = 0; i < keys.size(); ++i) {
        // Take this key's hash out of the ring *before* refilling its slot: the slot that holds
        // key i is the same one key i + depth goes into, so refilling first overwrites the hash
        // about to be used. The first version of this test did exactly that and the lookup missed.
        auto const ph = ring[i % depth];
        if (i + depth < keys.size()) {
            ring[i % depth] = map.prefetch(keys[i + depth]);
        }
        auto it = map.find(keys[i], ph);
        REQUIRE(it != map.end());
        sum += it->second;
    }
    REQUIRE(sum == 5000U * 4999U / 2U);
}

// An empty table has no bucket array at all -- data() is null -- so the prefetch has to stand down,
// and still answer with the hash, which is what hash_for does on the same table.
TEST_CASE("prefetch_on_a_table_without_buckets") {
    auto map = ankerl::unordered_dense::map<std::string, int>();
    REQUIRE(map.bucket_count() == 0U);
    REQUIRE(map.prefetch(std::string("anything")).m_mixed_hash == map.hash_for(std::string("anything")).m_mixed_hash);

    // and after the buckets go away again
    map["x"] = 1;
    map.clear();
    REQUIRE(map.prefetch(std::string("x")).m_mixed_hash == map.hash_for(std::string("x")).m_mixed_hash);
}

// The overload that takes a hash must win against the one that takes a key, or `prefetch(ph)` would
// try to hash the hash. Both are exact matches and the non-template wins, which is a rule worth
// pinning rather than trusting: the failure is silent if precomputed_hash ever grows a conversion.
TEST_CASE("prefetch_overload_on_a_precomputed_hash_returns_nothing") {
    auto map = ankerl::unordered_dense::map<std::string, int>();
    map["k"] = 1;
    auto const ph = map.hash_for(std::string("k"));
    static_assert(std::is_void_v<decltype(map.prefetch(ph))>,
                  "prefetch(precomputed_hash) is the overload that takes a hash, and it returns void");
    static_assert(std::is_same_v<decltype(map.prefetch(std::string("k"))),
                                 ankerl::unordered_dense::map<std::string, int>::precomputed_hash>,
                  "prefetch(key) hands back the hash it computed");
    map.prefetch(ph);
    REQUIRE(map.find(std::string("k"), ph) != map.end());
}

// Dropping the returned hash is the ordinary use of the one-argument form, so it must not be
// [[nodiscard]] -- a warning there would fire on correct code, and the suite is built with
// warnings as errors, which is what makes this a test rather than a comment.
TEST_CASE("prefetch_result_may_be_dropped") {
    auto map = ankerl::unordered_dense::map<uint64_t, int>();
    map[1] = 1;
    map.prefetch(uint64_t{1});
    map.prefetch(uint64_t{2});
    REQUIRE(map.size() == 1U);
}

// A transparent map takes a lookup type that is not the key type, the same way find does.
namespace {
// At namespace scope, not inside the test: gcc rejects the `is_avalanching` typedef as an unused
// local typedef when the struct is declared in a function body. Named for this file, because a
// unity build puts several test files in one translation unit.
struct prefetch_sv_hash {
    using is_transparent = void;
    using is_avalanching = void;
    auto operator()(std::string_view s) const -> uint64_t {
        return ankerl::unordered_dense::hash<std::string_view>{}(s);
    }
};
struct prefetch_sv_eq {
    using is_transparent = void;
    auto operator()(std::string_view a, std::string_view b) const -> bool {
        return a == b;
    }
};
} // namespace

TEST_CASE("prefetch_transparent") {
    auto map = ankerl::unordered_dense::map<std::string, int, prefetch_sv_hash, prefetch_sv_eq>();
    map["hello"] = 7;
    auto const ph = map.prefetch(std::string_view("hello"));
    REQUIRE(ph.m_mixed_hash == map.hash_for(std::string_view("hello")).m_mixed_hash);
    auto it = map.find(std::string_view("hello"), ph);
    REQUIRE(it != map.end());
    REQUIRE(it->second == 7);
}

// A set has the same lookup surface as a map.
TEST_CASE("prefetch_on_a_set") {
    auto set = ankerl::unordered_dense::set<uint64_t>();
    set.insert(42);
    auto const ph = set.prefetch(uint64_t{42});
    REQUIRE(ph.m_mixed_hash == set.hash_for(uint64_t{42}).m_mixed_hash);
    REQUIRE(set.count(uint64_t{42}, ph) == 1U);
}
