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

// The packing the table's own static_asserts check, spelled out here for the two shipped bucket
// types and for the deliberately tiny one below. A fingerprint that reaches dist_inc adds hash bits
// to the distance a bucket claims, which reorders the probe sequence without ever being wrong about
// a single lookup -- so nothing but this notices.
template <typename Bucket>
constexpr auto fingerprint_and_distance_do_not_overlap() -> bool {
    return Bucket::fingerprint_mask < Bucket::dist_inc && 0 != Bucket::dist_inc &&
           0 == (Bucket::dist_inc & (Bucket::dist_inc - 1));
}

static_assert(fingerprint_and_distance_do_not_overlap<ankerl::unordered_dense::bucket_type::standard>());
static_assert(fingerprint_and_distance_do_not_overlap<ankerl::unordered_dense::bucket_type::big>());

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
    static constexpr uint8_t fingerprint_mask = dist_inc - 1; // 11 bit = 2048 positions for distance

    uint8_t m_dist_and_fingerprint;
    uint8_t m_value_idx;
};

static_assert(fingerprint_and_distance_do_not_overlap<bucket_micro>());

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
