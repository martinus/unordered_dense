#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/hashers.h>

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

// The seven hashers below differ only in what they say about themselves, which is the whole point
// of each of them, so the body they share lives here once and the marker is the only line each one
// carries. They stay distinct types because two of them are named from outside by a specialization,
// which needs a type of its own to name.
struct identity {
    [[nodiscard]] auto operator()(int x) const noexcept -> uint64_t {
        return static_cast<uint64_t>(x);
    }
};

// Says nothing about itself.
struct quiet_hash : identity {};

// Says nothing either, and is called avalanching from outside (see the specializations below).
struct quiet_but_good_hash : identity {};

// Claims to be avalanching, and is contradicted from outside.
struct boastful_hash : identity {
    using is_avalanching = void;
};

// Claims to be avalanching and is taken at its word.
struct honest_hash : identity {
    using is_avalanching = void;
};

// The spelling Boost's documentation asks for, in both of its answers. A hash annotated for Boost
// has to be read the same way here -- and false_type in particular has to mean no, which a bare
// "the member is there" test would get backwards.
struct boost_style_yes : identity {
    using is_avalanching = std::true_type;
};

struct boost_style_no : identity {
    using is_avalanching = std::false_type;
};

// Any type carrying a compile time bool, not just the std:: ones -- std::true_type would not test
// this, being the same type as std::integral_constant<bool, true>.
struct home_grown_yes {
    static constexpr bool value = true;
};

struct home_grown_marker : identity {
    using is_avalanching = home_grown_yes;
};

// For the std::hash fallback: one type whose std::hash declares itself avalanching the way this
// library spells it, one the way Boost spells it, one that says nothing and is named from outside,
// and one that says nothing at all.
struct annotated {
    int x;
};

struct annotated_boost_style {
    int x;
};

struct named_from_outside {
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
struct std::hash<annotated_boost_style> {
    using is_avalanching = std::true_type;

    [[nodiscard]] auto operator()(annotated_boost_style const& a) const noexcept -> size_t {
        return static_cast<size_t>(a.x);
    }
};

template <>
struct std::hash<named_from_outside> {
    [[nodiscard]] auto operator()(named_from_outside const& a) const noexcept -> size_t {
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

// A std::hash that cannot be edited, named from outside -- the case the fallback used to miss.
template <>
struct ankerl::unordered_dense::hash_is_avalanching<std::hash<named_from_outside>> : std::true_type {};

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
    REQUIRE(ankerl::unordered_dense::hash_is_avalanching_v<home_grown_marker>);

    // The one that a mere "the member exists" test gets backwards.
    REQUIRE_FALSE(ankerl::unordered_dense::hash_is_avalanching_v<boost_style_no>);

    // And it decides what the table does, not just what the trait reports.
    REQUIRE(finalized<boost_style_yes>(7) == 7);
    REQUIRE(finalized<home_grown_marker>(7) == 7);
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

// The fallback used to read std::hash<T>::is_avalanching itself, which made it a second reader of
// the marker that understood only the one spelling. Both of these answered no before it was made to
// ask hash_is_avalanching like everything else.
TEST_CASE("the_fallback_reads_the_marker_the_same_way_as_everything_else") {
    using boost_style = ankerl::unordered_dense::hash<annotated_boost_style>;
    using outside = ankerl::unordered_dense::hash<named_from_outside>;

    REQUIRE(ankerl::unordered_dense::hash_is_avalanching_v<boost_style>);
    REQUIRE(ankerl::unordered_dense::hash_is_avalanching_v<outside>);

    auto boost_map = ankerl::unordered_dense::map<annotated_boost_style, int, boost_style>();
    REQUIRE(boost_map.hash_for(annotated_boost_style{7}).m_mixed_hash == 7);

    auto outside_map = ankerl::unordered_dense::map<named_from_outside, int, outside>();
    REQUIRE(outside_map.hash_for(named_from_outside{7}).m_mixed_hash == 7);
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

// The third thing mixed_hash can do, and the one nothing was asking for. An avalanching hash that
// is *narrower* than 64 bits has good bits, but only in the low half -- and the table indexes with
// `hash >> m_shifts`, which reads the top. So it is multiplied up first. Drop the multiply and
// every key lands in bucket zero; the table still answers every question correctly, just by
// probing linearly through the whole array, which is why only this can see it.
namespace {} // namespace

TEST_CASE("a_narrow_avalanching_hash_is_spread_into_the_high_bits") {
    // Not wyhash -- it said it avalanches and is believed.
    REQUIRE(finalized<test::narrow_avalanching_hash>(7) != ankerl::unordered_dense::detail::wyhash::hash(7));

    // ... but not used as it is either, which is what the third branch is for.
    auto const raw = static_cast<uint64_t>(test::narrow_avalanching_hash{}(7));
    REQUIRE(finalized<test::narrow_avalanching_hash>(7) != raw);
    REQUIRE(finalized<test::narrow_avalanching_hash>(7) == raw * UINT64_C(0x9ddfea08eb382d69));

    // A 64 bit hash of the same standing is used exactly as it is, which is the branch either
    // side. honest_hash is the identity, so "used as it is" and "equals the key" are the same
    // statement here.
    REQUIRE(finalized<honest_hash>(7) == honest_hash{}(7));

    // The bits that matter are the top ones, because that is where the bucket index is read from:
    // do_find indexes its first probe with hash >> m_shifts. Unspread, every one of these keys has
    // zeros up there, they all share a home bucket, and the table answers every lookup correctly by
    // walking most of the array -- which is why only a distribution check can see it.
    auto high_bits = ankerl::unordered_dense::set<uint64_t>();
    for (int i = 0; i < 1000; ++i) {
        high_bits.insert(finalized<test::narrow_avalanching_hash>(i) >> 40U);
    }
    REQUIRE(high_bits.size() > 900);

    // And a table built on it works, which is the other half: the spread above is worth nothing if
    // the finalization the table applies is not the one measured here.
    auto map = ankerl::unordered_dense::map<int, int, test::narrow_avalanching_hash, std::equal_to<int>>();
    for (int i = 0; i < 1000; ++i) {
        map[i] = i;
    }
    REQUIRE(map.size() == 1000);
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(map.at(i) == i);
    }
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
