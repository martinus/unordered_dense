#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstddef>     // for size_t
#include <cstdint>     // for uint64_t
#include <functional>  // for hash, equal_to
#include <string>      // for string
#include <string_view> // for string_view
#include <type_traits> // for true_type, false_type

// Issue #92. Whether a hash is high quality decides whether the table indexes with its bits as they
// come or mixes them first, and until now the only way to say so was a member typedef on the hash
// -- no good for a hash you did not write. hash_is_avalanching is the answer the table actually
// asks for, and it can be specialized from outside for either answer.

namespace {

// Says nothing about itself.
struct quiet_hash {
    [[nodiscard]] auto operator()(int x) const noexcept -> uint64_t {
        return static_cast<uint64_t>(x);
    }
};

// Same hash, and claims to be avalanching from outside (see the specialization below).
struct quiet_but_good_hash {
    [[nodiscard]] auto operator()(int x) const noexcept -> uint64_t {
        return static_cast<uint64_t>(x);
    }
};

// Claims to be avalanching, and is contradicted from outside.
struct boastful_hash {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(int x) const noexcept -> uint64_t {
        return static_cast<uint64_t>(x);
    }
};

// Claims to be avalanching and is taken at its word.
struct honest_hash {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(int x) const noexcept -> uint64_t {
        return static_cast<uint64_t>(x);
    }
};

// The spelling Boost's documentation asks for, in both of its answers. A hash annotated for Boost
// has to be read the same way here -- and false_type in particular has to mean no, which a bare
// "the member is there" test would get backwards.
struct boost_style_yes {
    using is_avalanching = std::true_type;

    [[nodiscard]] auto operator()(int x) const noexcept -> uint64_t {
        return static_cast<uint64_t>(x);
    }
};

struct boost_style_no {
    using is_avalanching = std::false_type;

    [[nodiscard]] auto operator()(int x) const noexcept -> uint64_t {
        return static_cast<uint64_t>(x);
    }
};

// Not std::true_type, but still a compile time bool.
struct boost_style_constant {
    using is_avalanching = std::integral_constant<bool, true>;

    [[nodiscard]] auto operator()(int x) const noexcept -> uint64_t {
        return static_cast<uint64_t>(x);
    }
};

// For the std::hash fallback: one type whose std::hash declares itself avalanching, one whose does
// not.
struct annotated {
    int x;
};

struct unannotated {
    int x;
};

// Transparent, to check that require_avalanching does not swallow the property.
struct transparent_string_hash {
    using is_transparent = void;
    using is_avalanching = void;

    [[nodiscard]] auto operator()(std::string_view str) const noexcept -> uint64_t {
        return ankerl::unordered_dense::hash<std::string_view>{}(str);
    }
};

} // namespace

template <>
struct std::hash<annotated> {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(annotated const& a) const noexcept -> size_t {
        return static_cast<size_t>(a.x);
    }
};

template <>
struct std::hash<unannotated> {
    [[nodiscard]] auto operator()(unannotated const& a) const noexcept -> size_t {
        return static_cast<size_t>(a.x);
    }
};

template <>
struct ankerl::unordered_dense::hash_is_avalanching<quiet_but_good_hash> : std::true_type {};

template <>
struct ankerl::unordered_dense::hash_is_avalanching<boastful_hash> : std::false_type {};

namespace {

template <typename Hash>
using map_with = ankerl::unordered_dense::map<int, int, Hash, std::equal_to<int>>;

// What the table finalizes a key's hash to, which is the thing the trait decides. An avalanching
// 64 bit hash is used as it is; anything else goes through wyhash first.
template <typename Hash>
auto finalized(int key) -> uint64_t {
    return map_with<Hash>().hash_for(key).m_mixed_hash;
}

} // namespace

TEST_CASE("the_member_typedef_still_answers") {
    REQUIRE_FALSE(ankerl::unordered_dense::hash_is_avalanching_v<quiet_hash>);
    REQUIRE(ankerl::unordered_dense::hash_is_avalanching_v<boastful_hash const>);
    REQUIRE(ankerl::unordered_dense::hash_is_avalanching_v<ankerl::unordered_dense::hash<std::string>>);
}

// Boost's boost::hash_is_avalanching reads the member as a compile time bool, and takes void only
// as a deprecated spelling of true. A hash annotated for one library has to mean the same in the
// other, or sharing hash types between them silently changes what the table does.
TEST_CASE("the_member_typedef_may_be_spelled_the_way_boost_asks") {
    REQUIRE(ankerl::unordered_dense::hash_is_avalanching_v<boost_style_yes>);
    REQUIRE(ankerl::unordered_dense::hash_is_avalanching_v<boost_style_constant>);

    // The one that a mere "the member exists" test gets backwards.
    REQUIRE_FALSE(ankerl::unordered_dense::hash_is_avalanching_v<boost_style_no>);

    // And it decides what the table does, not just what the trait reports.
    REQUIRE(finalized<boost_style_yes>(7) == 7);
    REQUIRE(finalized<boost_style_constant>(7) == 7);
    REQUIRE(finalized<boost_style_no>(7) == ankerl::unordered_dense::detail::wyhash::hash(7));
}

// Part one of the issue: specializing std::hash rather than moving the hash into ankerl's namespace
// should still get the trait noticed.
TEST_CASE("an_avalanching_std_hash_is_noticed_through_the_fallback") {
    REQUIRE(ankerl::unordered_dense::hash_is_avalanching_v<ankerl::unordered_dense::hash<annotated>>);
    REQUIRE_FALSE(ankerl::unordered_dense::hash_is_avalanching_v<ankerl::unordered_dense::hash<unannotated>>);

    // ... and it is not merely reported: an avalanching one is used unmixed.
    auto annotated_map = ankerl::unordered_dense::map<annotated, int, ankerl::unordered_dense::hash<annotated>>();
    REQUIRE(annotated_map.hash_for(annotated{7}).m_mixed_hash == 7);
}

// The specialization has to change what the table does, not just what a trait says.
TEST_CASE("saying_a_hash_is_avalanching_from_outside_stops_the_mixing") {
    REQUIRE(ankerl::unordered_dense::hash_is_avalanching_v<quiet_but_good_hash>);

    REQUIRE(finalized<quiet_but_good_hash>(7) == 7);
    REQUIRE(finalized<quiet_hash>(7) == ankerl::unordered_dense::detail::wyhash::hash(7));
    REQUIRE(finalized<quiet_hash>(7) != 7);
}

// And the other direction: a hash that claims to be good can be overruled.
TEST_CASE("saying_a_hash_is_not_avalanching_from_outside_starts_the_mixing") {
    REQUIRE_FALSE(ankerl::unordered_dense::hash_is_avalanching_v<boastful_hash>);
    REQUIRE(finalized<boastful_hash>(7) == ankerl::unordered_dense::detail::wyhash::hash(7));
}

// Whatever the trait says, the table has to keep working.
TEST_CASE("a_table_works_the_same_whichever_answer_the_trait_gives") {
    auto quiet = map_with<quiet_hash>();
    auto good = map_with<quiet_but_good_hash>();
    auto boastful = map_with<boastful_hash>();

    for (int i = 0; i < 1000; ++i) {
        quiet[i] = i;
        good[i] = i;
        boastful[i] = i;
    }
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(quiet.at(i) == i);
        REQUIRE(good.at(i) == i);
        REQUIRE(boastful.at(i) == i);
    }
    REQUIRE(quiet.size() == 1000);
    REQUIRE(good.size() == 1000);
    REQUIRE(boastful.size() == 1000);
}

TEST_CASE("require_avalanching_accepts_a_hash_that_says_so_itself") {
    using required = ankerl::unordered_dense::require_avalanching<honest_hash>;
    static_assert(ankerl::unordered_dense::hash_is_avalanching_v<required>);

    auto map = map_with<required>();
    for (int i = 0; i < 100; ++i) {
        map[i] = i;
    }
    REQUIRE(map.size() == 100);
    REQUIRE(map.at(42) == 42);
    REQUIRE(map.hash_for(7).m_mixed_hash == 7);
}

// It asks the trait, not the hash, so a hash overruled from outside is rejected rather than
// quietly requiring nothing. Only the condition is checked here -- the failure itself is a build
// error, which no test can catch at runtime:
//
//     require_avalanching<boastful_hash>   // static_assert fires
TEST_CASE("require_avalanching_would_reject_a_hash_the_trait_calls_bad") {
    REQUIRE_FALSE(ankerl::unordered_dense::hash_is_avalanching_v<boastful_hash>);
    REQUIRE_FALSE(ankerl::unordered_dense::hash_is_avalanching_v<quiet_hash>);
}

// The case the member typedef cannot carry on its own: a hash named avalanching only from outside.
TEST_CASE("require_avalanching_accepts_a_hash_named_avalanching_by_the_trait") {
    using required = ankerl::unordered_dense::require_avalanching<quiet_but_good_hash>;
    static_assert(ankerl::unordered_dense::hash_is_avalanching_v<required>);

    auto map = map_with<required>();
    map[7] = 7;
    REQUIRE(map.at(7) == 7);
    REQUIRE(map.hash_for(7).m_mixed_hash == 7);
}

// Wrapping must not cost the hash anything else it declared.
TEST_CASE("require_avalanching_keeps_the_hash_transparent") {
    using required = ankerl::unordered_dense::require_avalanching<transparent_string_hash>;
    auto map = ankerl::unordered_dense::map<std::string, int, required, std::equal_to<>>();
    map["hello"] = 1;

    REQUIRE(map.find(std::string_view("hello")) != map.end());
    REQUIRE(map.contains(std::string_view("hello")));
    REQUIRE(map.hash_for(std::string_view("hello")).m_mixed_hash == map.hash_for(std::string("hello")).m_mixed_hash);
}

// A stateful hash still gets its state through the wrapper, which is an aggregate over it.
TEST_CASE("require_avalanching_can_be_given_a_hash_to_copy") {
    struct seeded_hash {
        using is_avalanching = void;
        uint64_t m_seed{};

        [[nodiscard]] auto operator()(int x) const noexcept -> uint64_t {
            return ankerl::unordered_dense::detail::wyhash::hash(static_cast<uint64_t>(x) + m_seed);
        }
    };

    using required = ankerl::unordered_dense::require_avalanching<seeded_hash>;
    auto one = map_with<required>(0, required{seeded_hash{1}});
    auto two = map_with<required>(0, required{seeded_hash{2}});

    REQUIRE(one.hash_for(7).m_mixed_hash != two.hash_for(7).m_mixed_hash);
    REQUIRE(one.hash_for(7).m_mixed_hash == ankerl::unordered_dense::detail::wyhash::hash(8));
}
