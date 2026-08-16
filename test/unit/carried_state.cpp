#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/hashers.h>
#include <app/id_allocator.h>

#include <cstddef> // for size_t
#include <utility> // for move, pair

// A table carries more than its elements. The hasher, the key equality and the maximum load factor
// travel with it, and each of them is copied, moved or swapped by hand -- one statement per member,
// in four different places (copy assignment, both branches of move assignment, and swap).
//
// A deletion sweep of the header removed each of those statements in turn and nothing went red.
// That is not surprising in hindsight: with std::hash and std::equal_to, which every other test
// uses, a table that forgets its hasher is indistinguishable from one that kept it, because there
// is nothing to tell the two instances apart. So the tests here give the table a hasher and an
// equality that carry an inert tag (test::tagged_hash, test::tagged_equal), which is the whole
// reason those types exist, and then ask the table which one it ended up with.
//
// Losing the max load factor is the one with teeth even without a tag: it decides when the table
// grows, so a swap that keeps the old one makes the table rehash at the wrong size for the rest of
// its life.

namespace {

// Distinct on purpose, and distinct from the default 0, so that "kept its own" and "took the
// other's" cannot both pass.
constexpr auto tag_a = 11;
constexpr auto tag_b = 22;

constexpr auto load_a = 0.5F;
constexpr auto load_b = 0.9F;

template <typename Map>
auto make(int hash_tag, int equal_tag, float load) -> Map {
    auto map = Map(0, test::tagged_hash{hash_tag}, test::tagged_equal{equal_tag});
    map.max_load_factor(load);
    for (int i = 0; i < 20; ++i) {
        map.try_emplace(i, i);
    }
    return map;
}

// The three things a table carries besides its elements, asked for together because every operation
// below has to preserve all three and it is the *combination* that keeps being got wrong -- a swap
// that exchanges the hasher and forgets the load factor passes any test that looks at one of them.
template <typename Map>
void require_carries(Map const& map, int hash_tag, int equal_tag, float load) {
    REQUIRE(map.hash_function().tag == hash_tag);
    REQUIRE(map.key_eq().tag == equal_tag);
    REQUIRE(map.max_load_factor() == load);
}

// ... and it is still the table it was: the tag takes no part in hashing, so nothing above should
// have changed what the table can find.
template <typename Map>
void require_still_finds_everything(Map const& map) {
    REQUIRE(map.size() == 20);
    for (int i = 0; i < 20; ++i) {
        auto it = map.find(i);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i);
    }
}

} // namespace

TEST_CASE_MAP("swap_exchanges_the_carried_state", int, int, test::tagged_hash, test::tagged_equal) {
    auto a = make<map_t>(tag_a, tag_a, load_a);
    auto b = make<map_t>(tag_b, tag_b, load_b);

    a.swap(b);

    require_carries(a, tag_b, tag_b, load_b);
    require_carries(b, tag_a, tag_a, load_a);
    require_still_finds_everything(a);
    require_still_finds_everything(b);

    // and back, because a swap that assigns in one direction only would pass the above if both
    // tables happened to be asked in the same order.
    swap(a, b);
    require_carries(a, tag_a, tag_a, load_a);
    require_carries(b, tag_b, tag_b, load_b);
}

TEST_CASE_MAP("copy_assignment_takes_the_carried_state", int, int, test::tagged_hash, test::tagged_equal) {
    auto a = make<map_t>(tag_a, tag_a, load_a);
    auto const b = make<map_t>(tag_b, tag_b, load_b);

    a = b;

    require_carries(a, tag_b, tag_b, load_b);
    require_still_finds_everything(a);

    // the source is const and must be untouched by having been copied from
    require_carries(b, tag_b, tag_b, load_b);
}

TEST_CASE_MAP("move_assignment_takes_the_carried_state", int, int, test::tagged_hash, test::tagged_equal) {
    auto a = make<map_t>(tag_a, tag_a, load_a);
    auto b = make<map_t>(tag_b, tag_b, load_b);

    a = std::move(b);

    require_carries(a, tag_b, tag_b, load_b);
    require_still_finds_everything(a);
}

TEST_CASE_MAP("copy_construction_takes_the_carried_state", int, int, test::tagged_hash, test::tagged_equal) {
    auto const a = make<map_t>(tag_a, tag_a, load_a);
    auto const copy = a;
    require_carries(copy, tag_a, tag_a, load_a);
    require_still_finds_everything(copy);
}

TEST_CASE_MAP("move_construction_takes_the_carried_state", int, int, test::tagged_hash, test::tagged_equal) {
    auto a = make<map_t>(tag_a, tag_a, load_a);
    auto const moved = std::move(a);
    require_carries(moved, tag_a, tag_a, load_a);
    require_still_finds_everything(moved);
}

// The growth threshold is the fourth thing a table carries, and the only one with no getter: it is
// the size at which the table decides it is full. It belongs to the bucket array, so an operation
// that takes the array and leaves the threshold behind builds a table that rehashes at the wrong
// moment -- while answering every question about its hasher, its equality, its load factor and its
// contents correctly. What makes it visible is bucket_count() holding still while the table
// demonstrably still has room: a table that kept a threshold meant for a much smaller array grows
// on the first few inserts instead.
namespace {

// Big enough that the inserts below cannot legitimately fill it, so a bucket_count that moves is
// the bug and not the load factor doing its job.
constexpr auto reserved = 1000;
constexpr auto comfortably_inside = 500;

} // namespace

TEST_CASE_MAP("swap_exchanges_the_growth_threshold", int, int, test::tagged_hash, test::tagged_equal) {
    auto big = map_t();
    big.reserve(reserved);
    auto const big_buckets = big.bucket_count();

    auto small = map_t();
    small.try_emplace(1, 1);

    small.swap(big);
    REQUIRE(small.bucket_count() == big_buckets);

    for (int i = 0; i < comfortably_inside; ++i) {
        small.try_emplace(i, i);
    }
    REQUIRE(small.bucket_count() == big_buckets);
}

TEST_CASE_MAP("move_assignment_takes_the_growth_threshold", int, int, test::tagged_hash, test::tagged_equal) {
    auto big = map_t();
    big.reserve(reserved);
    auto const big_buckets = big.bucket_count();

    auto target = map_t();
    target.try_emplace(1, 1);

    target = std::move(big);
    REQUIRE(target.bucket_count() == big_buckets);

    for (int i = 0; i < comfortably_inside; ++i) {
        target.try_emplace(i, i);
    }
    REQUIRE(target.bucket_count() == big_buckets);
}

// Move assignment has two halves and they carry the state separately. Everything above reaches the
// first one, where the allocators compare equal and the bucket array can simply be taken over. When
// they differ the table cannot adopt the other's memory, so it copies the buckets instead and
// carries the hasher, the equality and the load factor over in three more statements of their own.
// std::allocator makes that branch unreachable -- it is stateless and every instance compares equal
// -- which is why it needs an allocator whose instances differ.
namespace {

using carried_state_alloc = test::pmr_like_allocator<std::pair<int, int>>;
using carried_state_map = ankerl::unordered_dense::map<int, int, test::tagged_hash, test::tagged_equal, carried_state_alloc>;

} // namespace

TEST_CASE("move_assignment_between_unequal_allocators_takes_the_carried_state") {
    auto a = carried_state_map(0, test::tagged_hash{tag_a}, test::tagged_equal{tag_a}, carried_state_alloc(1));
    auto b = carried_state_map(0, test::tagged_hash{tag_b}, test::tagged_equal{tag_b}, carried_state_alloc(2));
    a.max_load_factor(load_a);
    b.max_load_factor(load_b);
    for (int i = 0; i < 20; ++i) {
        b.try_emplace(i, i);
    }

    // the branch this test exists for: neither allocator propagates and the two do not compare
    // equal, so the move cannot take the other's buckets.
    REQUIRE(!(a.get_allocator() == b.get_allocator()));

    a = std::move(b);

    require_carries(a, tag_b, tag_b, load_b);
    require_still_finds_everything(a);
    REQUIRE(a.get_allocator().m_id == 1); // and it kept its own allocator, which is what made the branch run
}
