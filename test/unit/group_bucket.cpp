#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// The index: sixteen fingerprints and eight overflow counters per group, value indices beside
// them. The whole suite runs on it, since it is the only index there is; what is here is what
// only makes sense to ask of it directly.

using map_t = ankerl::unordered_dense::map<std::string, size_t>;

// max_size() is a multiple of sixteen, so the largest array is whole groups
static_assert(map_t::max_bucket_count() % 16U == 0U);

TEST_CASE("group_bucket_count_is_in_slots") {
    auto map = map_t();
    REQUIRE(map.bucket_count() == 0U);
    map["a"] = 1;
    // the smallest array: four groups
    REQUIRE(map.bucket_count() == 64U);
    REQUIRE(map.bucket_count() % 16U == 0U);
    map.reserve(1000);
    REQUIRE(map.bucket_count() >= 1000U / map.max_load_factor());
    REQUIRE(map.bucket_count() % 16U == 0U);
    REQUIRE(map.load_factor() < map.max_load_factor());
}

// index_bytes() is the index's own size, which nothing else in the interface can give: a bucket is
// a group of sixteen slots, and `bucket_type` is only the 24 bytes of it the probe compares, so
// `bucket_count() * sizeof(bucket_type)` reads 24 bytes per slot for an index that costs 5.5. The
// checks below are against what the allocator was really asked for, not against that arithmetic
// repeated.
namespace {

// Live bytes of every allocation the size of one block, which is the index and, since the value
// type here is sixteen bytes, only the index.
[[nodiscard]] auto index_live_bytes() -> std::size_t& {
    static auto bytes = std::size_t{0};
    return bytes;
}

template <typename T, std::size_t BlockBytes>
class index_watching_allocator {
    template <typename U, std::size_t B>
    friend class index_watching_allocator;

public:
    using value_type = T;

    // Spelled out because the block size is a non-type parameter, which the default rebind of
    // std::allocator_traits cannot carry: it only replaces the first *type* argument.
    template <typename U>
    struct rebind {
        using other = index_watching_allocator<U, BlockBytes>;
    };

    index_watching_allocator() noexcept = default;

    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    index_watching_allocator(index_watching_allocator<U, BlockBytes> const& /*other*/) noexcept {}

    auto allocate(std::size_t n) -> T* {
        if (sizeof(T) == BlockBytes) {
            index_live_bytes() += n * sizeof(T);
        }
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, std::size_t n) noexcept {
        if (sizeof(T) == BlockBytes) {
            index_live_bytes() -= n * sizeof(T);
        }
        std::allocator<T>{}.deallocate(p, n);
    }

    template <typename U>
    friend auto operator==(index_watching_allocator const& /*a*/,
                           index_watching_allocator<U, BlockBytes> const& /*b*/) noexcept -> bool {
        return true;
    }

    template <typename U>
    friend auto operator!=(index_watching_allocator const& /*a*/,
                           index_watching_allocator<U, BlockBytes> const& /*b*/) noexcept -> bool {
        return false;
    }
};

// One block is the group plus one value index per slot: 88 bytes for `group`, 152 for `group_big`.
template <typename Bucket>
constexpr std::size_t block_bytes_v = sizeof(Bucket) + 16U * sizeof(typename Bucket::value_idx_type);

template <typename Bucket>
using watched_map_t = ankerl::unordered_dense::map<
    uint64_t,
    uint64_t,
    ankerl::unordered_dense::hash<uint64_t>,
    std::equal_to<uint64_t>,
    std::vector<std::pair<uint64_t, uint64_t>, index_watching_allocator<std::pair<uint64_t, uint64_t>, block_bytes_v<Bucket>>>,
    Bucket>;

static_assert(block_bytes_v<ankerl::unordered_dense::bucket_type::group> == 88U);
#if SIZE_MAX == UINT32_MAX
// a wide value index is size_t wide, so on a 32 bit target it is the same four bytes as the narrow
// one and both blocks are 88
static_assert(block_bytes_v<ankerl::unordered_dense::bucket_type::group_big> == 88U);
#else
static_assert(block_bytes_v<ankerl::unordered_dense::bucket_type::group_big> == 152U);
#endif
// ... so that the allocator above cannot mistake a pair of uint64_t for a block
static_assert(sizeof(std::pair<uint64_t, uint64_t>) != 88U && sizeof(std::pair<uint64_t, uint64_t>) != 152U);

template <typename Bucket>
void index_bytes_is_what_the_allocator_gave() {
    constexpr auto bytes_per_sixteen_slots = block_bytes_v<Bucket>;

    index_live_bytes() = 0;
    auto map = watched_map_t<Bucket>();

    // an index that does not exist costs nothing
    REQUIRE(map.index_bytes() == 0U);
    REQUIRE(index_live_bytes() == 0U);

    for (uint64_t i = 0; i < 10000; ++i) {
        map[i] = i;
        INFO("i=", i);
        REQUIRE(map.index_bytes() == index_live_bytes());
        // and it is the whole array, not one group of it
        REQUIRE(map.index_bytes() * 16U == map.bucket_count() * bytes_per_sixteen_slots);
    }

    // through a rehash that shrinks, and through one that grows
    map.rehash(0);
    REQUIRE(map.index_bytes() == index_live_bytes());
    map.reserve(1000000);
    REQUIRE(map.index_bytes() == index_live_bytes());
    REQUIRE(map.index_bytes() * 16U == map.bucket_count() * bytes_per_sixteen_slots);

    // and back to nothing when the index is given back
    map = watched_map_t<Bucket>();
    REQUIRE(map.index_bytes() == 0U);
    REQUIRE(index_live_bytes() == 0U);
}

} // namespace

TEST_CASE("index_bytes_against_the_allocator") {
    // 88 bytes per sixteen slots, which is 5.5 per slot, and 152 where the wide value index is
    // really wider
    index_bytes_is_what_the_allocator_gave<ankerl::unordered_dense::bucket_type::group>();
    index_bytes_is_what_the_allocator_gave<ankerl::unordered_dense::bucket_type::group_big>();
}

// Against std::unordered_map, with every operation that touches the index: the overflow counters
// have to come back down on erase, or a later probe walks on past the group it should have
// stopped in and a later insert lands where a probe cannot find it. A run that grows and one
// that reserves first, so both the growth rehash and a table that never grows are covered.
namespace {

template <typename Map>
void run_against_reference(unsigned seed, uint64_t range, size_t ops, bool reserve_first) {
    auto rng = std::mt19937_64(seed);
    auto map = Map();
    auto ref = std::unordered_map<uint64_t, uint64_t>();
    if (reserve_first) {
        map.reserve(static_cast<size_t>(range / 2));
    }
    for (size_t op = 0; op < ops; ++op) {
        auto const key = rng() % range;
        switch (rng() % 8U) {
        case 0:
        case 1: {
            REQUIRE(map.try_emplace(key, op).second == ref.try_emplace(key, op).second);
            break;
        }
        case 2: {
            REQUIRE(map.erase(key) == ref.erase(key));
            break;
        }
        case 3: {
            // erase by iterator, which locates the slot from the value rather than from a probe
            auto it = map.find(key);
            if (it != map.end()) {
                map.erase(it);
                ref.erase(key);
            }
            break;
        }
        case 4: {
            if (op % 1000 == 0) {
                auto copy = map;
                map = std::move(copy);
            }
            break;
        }
        case 5: {
            if (op % 5000 == 0) {
                map.rehash(0);
            }
            break;
        }
        case 6: {
            // replace_key moves an entry between probe sequences without touching the values
            auto it = map.find(key);
            if (it != map.end()) {
                auto const new_key = key + range;
                auto const r = map.replace_key(it, new_key);
                if (r.second) {
                    auto const v = ref.at(key);
                    ref.erase(key);
                    ref[new_key] = v;
                }
            }
            break;
        }
        default: {
            auto a = map.find(key);
            auto b = ref.find(key);
            REQUIRE((a != map.end()) == (b != ref.end()));
            if (a != map.end()) {
                REQUIRE(a->second == b->second);
            }
            break;
        }
        }
        REQUIRE(map.size() == ref.size());
    }
    for (auto const& kv : map) {
        auto it = ref.find(kv.first);
        REQUIRE(it != ref.end());
        REQUIRE(it->second == kv.second);
    }
    for (auto const& kv : ref) {
        auto it = map.find(kv.first);
        REQUIRE(it != map.end());
        REQUIRE(it->second == kv.second);
    }
}

template <typename Bucket>
using u64_map = ankerl::unordered_dense::map<uint64_t,
                                             uint64_t,
                                             ankerl::unordered_dense::hash<uint64_t>,
                                             std::equal_to<uint64_t>,
                                             std::allocator<std::pair<uint64_t, uint64_t>>,
                                             Bucket>;

} // namespace

// The bucket count reserve() picks is the *smallest* power of two that holds the request at the
// maximum load factor, and where the boundary falls is decided by one comparison in
// calc_shifts_for_size(). Nothing pinned it: turning that `<` into `<=` moves every exact boundary
// by one element and doubles the array a step early, which costs memory on every table built with
// reserve() and which a mutation sweep on 2026-09-10 found nothing noticed.
//
// So this checks both sides: the count is enough, and half of it would not have been. The listed
// sizes are the exact boundaries at a load factor of 0.8 -- 64 slots hold 51, 128 hold 102, and so
// on -- because a size in the middle of a range is satisfied by both answers.
TEST_CASE("reserve_picks_the_smallest_array_that_holds_the_request") {
    using sized_map_t = ankerl::unordered_dense::map<uint64_t, uint64_t>;

    for (auto n : {size_t{1}, size_t{51}, size_t{52}, size_t{102}, size_t{103}, size_t{204}, size_t{205}, size_t{1000}}) {
        auto map = sized_map_t();
        map.reserve(n);
        auto const count = map.bucket_count();
        INFO("reserve(", n, ") gave ", count, " buckets");

        // enough for the request ...
        REQUIRE(static_cast<double>(count) * static_cast<double>(map.max_load_factor()) >= static_cast<double>(n));
        // ... and the next size down would not have been, unless we are already at the smallest
        if (count > 64) {
            REQUIRE(static_cast<double>(count / 2) * static_cast<double>(map.max_load_factor()) < static_cast<double>(n));
        }

        // and it really is room rather than a number: filling to the request must not grow it
        for (uint64_t i = 0; i < n; ++i) {
            map[i] = i;
        }
        REQUIRE(map.bucket_count() == count);
    }
}

TEST_CASE("group_index_against_reference") {
    run_against_reference<u64_map<ankerl::unordered_dense::bucket_type::group>>(1, 10, 20000, false);
    run_against_reference<u64_map<ankerl::unordered_dense::bucket_type::group>>(2, 3000, 200000, false);
    run_against_reference<u64_map<ankerl::unordered_dense::bucket_type::group>>(3, 100000, 300000, true);
    run_against_reference<u64_map<ankerl::unordered_dense::bucket_type::group_big>>(4, 3000, 200000, false);
}
