#if 0
#include <ankerl/unordered_dense.h>

#include <app/Counter.h>

#include <doctest.h>
#include <fmt/format.h>

#include <limits>
#include <stdexcept> // for out_of_range

using MapDefault = ankerl::unordered_dense::map<std::string, size_t>;

// big bucket type allows 2^64 elements, but has more memory & CPU overhead.
using MapBig = ankerl::unordered_dense::map<std::string,
                                            size_t,
                                            ankerl::unordered_dense::hash<std::string>,
                                            std::equal_to<std::string>,
                                            std::allocator<std::pair<std::string, size_t>>,
                                            ankerl::unordered_dense::bucket_type::big>;

static_assert(sizeof(MapDefault::bucket_type) == 8U);
static_assert(sizeof(MapBig::bucket_type) == sizeof(size_t) + 4U);
static_assert(MapDefault{}.max_size() == MapDefault::max_bucket_count());

#if SIZE_MAX == UINT32_MAX
static_assert(MapDefault::max_size() == uint64_t{1} << 31U);
static_assert(MapBig::max_size() == uint64_t{1} << 31U);
#else
static_assert(MapDefault::max_size() == uint64_t{1} << 32U);
static_assert(MapBig::max_size() == uint64_t{1} << 63U);
#endif

struct bucket_micro {
    static constexpr uint8_t DIST_INC = 1U << 1U;             // 1 bits for fingerprint
    static constexpr uint8_t FINGERPRINT_MASK = DIST_INC - 1; // 11 bit = 2048 positions for distance

    uint8_t dist_and_fingerprint;
    uint8_t value_idx;
};

TEST_CASE("bucket_micro") {
    using Map = ankerl::unordered_dense::map<Counter::Obj,
                                             Counter::Obj,
                                             ankerl::unordered_dense::hash<Counter::Obj>,
                                             std::equal_to<Counter::Obj>,
                                             std::allocator<std::pair<Counter::Obj, Counter::Obj>>,
                                             bucket_micro>;

    Counter counts;
    INFO(counts);

    auto map = Map();
    for (size_t i = 0; i < Map::max_size(); ++i) {
        auto const r = map.try_emplace({i, counts}, i, counts);
        REQUIRE(r.second);

        auto it = map.find({0, counts});
        REQUIRE(it != map.end());
    }
    REQUIRE_THROWS_AS(map.try_emplace({Map::max_size(), counts}, Map::max_size(), counts), std::overflow_error);

    // check that all elements are there
    REQUIRE(map.size() == Map::max_size());
    for (size_t i = 0; i < Map::max_size(); ++i) {
        INFO(i);
        auto it = map.find({i, counts});
        REQUIRE(it != map.end());
        REQUIRE(it->first.get() == i);
        REQUIRE(it->second.get() == i);
    }
}

#endif