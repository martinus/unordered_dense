#include <ankerl/unordered_dense.h>

#include <app/counter.h>
#include <app/doctest.h>

#include <fmt/format.h>

#include <limits>
#include <stdexcept> // for out_of_range

using map_default_t = ankerl::unordered_dense::map<std::string, size_t>;

// big bucket type allows 2^64 elements, but has more memory & CPU overhead.
using map_big_t = ankerl::unordered_dense::map<std::string,
                                               size_t,
                                               ankerl::unordered_dense::hash<std::string>,
                                               std::equal_to<std::string>,
                                               std::allocator<std::pair<std::string, size_t>>,
                                               ankerl::unordered_dense::bucket_type::big>;

// The packing is asserted by the table itself, so that a user's own bucket type is held to it
// too -- see the static_asserts on Bucket in the header. Instantiating map_default_t, map_big_t
// and the bucket_micro map below is what fires them for these three.
static_assert(sizeof(map_default_t::bucket_type) == 8U);
static_assert(sizeof(map_big_t::bucket_type) == sizeof(size_t) + 4U);
static_assert(map_default_t::max_size() == map_default_t::max_bucket_count());

#if SIZE_MAX == UINT32_MAX
static_assert(map_default_t::max_size() == uint64_t{1} << 31U);
static_assert(map_big_t::max_size() == uint64_t{1} << 31U);
#else
static_assert(map_default_t::max_size() == uint64_t{1} << 32U);
static_assert(map_big_t::max_size() == uint64_t{1} << 63U);
#endif

struct bucket_micro {
    static constexpr uint8_t dist_inc = 1U << 1U;             // 1 bits for fingerprint
    static constexpr uint8_t fingerprint_mask = dist_inc - 1; // 7 bit = 128 positions for distance

    uint8_t m_dist_and_fingerprint;
    uint8_t m_value_idx;
};

TYPE_TO_STRING_MAP(counter::obj,
                   counter::obj,
                   ankerl::unordered_dense::hash<counter::obj>,
                   std::equal_to<counter::obj>,
                   std::allocator<std::pair<counter::obj, counter::obj>>,
                   bucket_micro);

TEST_CASE_MAP("bucket_micro",
              counter::obj,
              counter::obj,
              ankerl::unordered_dense::hash<counter::obj>,
              std::equal_to<counter::obj>,
              std::allocator<std::pair<counter::obj, counter::obj>>,
              bucket_micro) {
    counter counts;
    INFO(counts);

    auto map = map_t();
    INFO("map_t::max_size()=" << map_t::max_size());
    for (size_t i = 0; i < map_t::max_size(); ++i) {
        if (i == 255) {
            INFO("i=" << i);
        }
        auto const r = map.try_emplace({i, counts}, i, counts);
        REQUIRE(r.second);

        auto it = map.find({0, counts});
        REQUIRE(it != map.end());
    }
    // NOLINTNEXTLINE(llvm-else-after-return,readability-else-after-return)
    REQUIRE_THROWS_AS(map.try_emplace({map_t::max_size(), counts}, map_t::max_size(), counts), std::overflow_error);

    // check that all elements are there
    REQUIRE(map.size() == map_t::max_size());
    for (size_t i = 0; i < map_t::max_size(); ++i) {
        INFO(i);
        auto it = map.find({i, counts});
        REQUIRE(it != map.end());
        REQUIRE(it->first.get() == i);
        REQUIRE(it->second.get() == i);
    }
}

// replace() refuses a container it could not index, and the boundary is the interesting part: the
// guard is `>`, so a container of exactly max_size() has to be accepted. bucket_micro is what makes
// that testable at all -- its value index is one byte, so max_size() is 256 rather than 2^32.
namespace {

using micro_map_t = ankerl::unordered_dense::map<size_t,
                                                 size_t,
                                                 ankerl::unordered_dense::hash<size_t>,
                                                 std::equal_to<size_t>,
                                                 std::allocator<std::pair<size_t, size_t>>,
                                                 bucket_micro>;

[[nodiscard]] auto container_of(size_t count) -> micro_map_t::value_container_type {
    auto container = micro_map_t::value_container_type{};
    for (size_t i = 0; i < count; ++i) {
        container.emplace_back(i, i);
    }
    return container;
}

} // namespace

TEST_CASE("replace_takes_exactly_max_size_and_refuses_one_more") {
    auto map = micro_map_t();
    map.replace(container_of(micro_map_t::max_size()));
    REQUIRE(map.size() == micro_map_t::max_size());
    for (size_t i = 0; i < micro_map_t::max_size(); ++i) {
        REQUIRE(map.contains(i));
    }

    auto too_many = micro_map_t();
    REQUIRE_THROWS_AS(too_many.replace(container_of(micro_map_t::max_size() + 1)), std::out_of_range);
}

// The duplicate-dropping loop closes the hole by moving the last element into it -- except when the
// duplicate *is* the last element, which is the branch nothing was reaching. A container whose tail
// repeats its head lands there on the final iteration.
TEST_CASE("replace_drops_a_duplicate_that_sits_last") {
    auto map = micro_map_t();
    auto container = micro_map_t::value_container_type{};
    container.emplace_back(1, 10);
    container.emplace_back(2, 20);
    container.emplace_back(1, 30); // the duplicate, and the last element

    map.replace(std::move(container));
    REQUIRE(map.size() == 2U);
    REQUIRE(map.contains(1));
    REQUIRE(map.contains(2));
    REQUIRE(map.at(2) == 20U);
    // The first of the duplicates is the one that stays; the later one is dropped, not merged.
    REQUIRE(map.at(1) == 10U);
}

// A count above what the bucket array can ever hold. calc_shifts_for_size() walks the shift down
// until the capacity it computes covers the count -- but the bucket count saturates at
// max_bucket_count(), so past max_bucket_count() * max_load_factor() the capacity being compared
// stops growing while the walk carries on, all the way to a shift of zero. calc_num_buckets(0) then
// asks for `1 << 64`, which is undefined and in practice one: a table sized for far more elements
// than it can hold came back with a single bucket and a mask of zero, and the next probe read past
// the end of it.
//
// bucket_micro is what makes this cheap to ask. The arithmetic is the same for the shipped bucket
// types -- map<uint32_t, uint32_t>::rehash(3865470566) reproduced it -- but there the correct
// answer is an array of 2^32 buckets, so the test would be asking the machine for 32 GB. Here the
// same walk runs off the same end and the correct answer is 256 buckets.
TEST_CASE("a_count_above_the_bucket_limit_does_not_collapse_the_array") {
    auto const above_the_limit = static_cast<size_t>(static_cast<double>(micro_map_t::max_bucket_count()) * double{0.8}) + 1;
    REQUIRE(above_the_limit < micro_map_t::max_size());

    SUBCASE("rehash") {
        // rehash() is the reachable way in: unlike reserve() it does not size the value container
        // first, so there is no enormous allocation to fail before the bucket arithmetic runs.
        auto map = micro_map_t();
        map[1] = 1;
        map.rehash(above_the_limit);
        REQUIRE(map.bucket_count() == micro_map_t::max_bucket_count());
        REQUIRE(map.find(1) != map.end());

        map[2] = 2;
        REQUIRE(map.find(2) != map.end());
        REQUIRE(map.size() == 2U);
    }

    SUBCASE("reserve") {
        auto map = micro_map_t();
        map[1] = 1;
        map.reserve(above_the_limit);
        REQUIRE(map.bucket_count() == micro_map_t::max_bucket_count());
        REQUIRE(map.find(1) != map.end());
    }

    SUBCASE("replace") {
        auto map = micro_map_t();
        map.replace(container_of(above_the_limit));
        REQUIRE(map.size() == above_the_limit);
        for (size_t i = 0; i < above_the_limit; ++i) {
            REQUIRE(map.contains(i));
        }
    }
}

// reserve() clamps what it is asked for to max_size() before handing it to the value container.
// Without that, a caller asking for more than the table could ever index reaches
// std::vector::reserve() with the raw number and gets a std::length_error out of a call that
// should simply have given them everything there is.
//
// bucket_micro is what makes this askable. On a default map max_size() is 2^32, so the clamped
// call still reserves 2^32 pairs -- tens of gigabytes -- and the test would be indistinguishable
// from the bug. Here the clamp lands on 256.
TEST_CASE("reserve_clamps_to_max_size_instead_of_failing") {
    auto map = micro_map_t();
    map.reserve((std::numeric_limits<size_t>::max)());

    REQUIRE(map.bucket_count() <= micro_map_t::max_bucket_count());
    REQUIRE(map.values().capacity() <= micro_map_t::max_size());

    // ... and it is a working table afterwards, not merely one that did not throw
    map[1] = 10;
    REQUIRE(map.at(1) == 10U);
    REQUIRE(map.size() == 1U);
}

// The same clamp in rehash() is *not* tested, and deliberately: calc_shifts_for_size() saturates
// at max_bucket_count(), so it answers the same for count and for min(count, max_size()) and the
// clamp cannot change anything. It is the reserve() one that reaches a container.

// rehash() hands the values container back whatever it is holding beyond its size. Nothing was
// checking that, because every other rehash test asks about buckets -- and the value container is
// the larger of the two allocations.
TEST_CASE("rehash_shrinks_the_value_container") {
    auto map = ankerl::unordered_dense::map<size_t, size_t>();
    map.reserve(1000);
    for (size_t i = 0; i < 10; ++i) {
        map[i] = i;
    }
    REQUIRE(map.values().capacity() >= 1000);

    map.rehash(0);

    REQUIRE(map.values().capacity() < 1000);
    REQUIRE(map.size() == 10U);
    for (size_t i = 0; i < 10; ++i) {
        REQUIRE(map.at(i) == i);
    }
}
