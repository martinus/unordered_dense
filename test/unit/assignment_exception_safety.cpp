#include <ankerl/unordered_dense.h>

#include <app/bombing_allocator.h>
#include <app/doctest.h>
#include <app/id_allocator.h>
#include <app/map_fixtures.h>

#include <cstddef>
#include <new>
#include <stdexcept>
#include <utility>

// Assignment gives the buckets back before it knows whether it can build new ones. A throw in
// between used to leave the table holding values with no bucket array: size() counted elements
// find() could not reach, and the next lookup indexed a container of length zero.
namespace {

// Throws on the nth copy, and only on copies -- the table has to be able to move it around while
// putting itself back together.
struct throws_on_copy {
    static inline int copies_until_throw = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    int m_value = 0;

    throws_on_copy() = default;

    explicit throws_on_copy(int value)
        : m_value(value) {}

    throws_on_copy(throws_on_copy const& other)
        : m_value(other.m_value) {
        if (0 < copies_until_throw && 0 == --copies_until_throw) {
            throw std::runtime_error("copy");
        }
    }

    throws_on_copy(throws_on_copy&&) noexcept = default;
    auto operator=(throws_on_copy&&) noexcept -> throws_on_copy& = default;
    ~throws_on_copy() = default;

    auto operator=(throws_on_copy const& other) -> throws_on_copy& {
        if (this != &other) {
            auto tmp = other; // may throw, which is the point
            m_value = tmp.m_value;
        }
        return *this;
    }
};

using map_t = ankerl::unordered_dense::map<int, throws_on_copy>;
using segmented_map_t = ankerl::unordered_dense::segmented_map<int, throws_on_copy>;

struct countdown_guard {
    explicit countdown_guard(int n) {
        throws_on_copy::copies_until_throw = n;
    }
    countdown_guard(countdown_guard const&) = delete;
    countdown_guard(countdown_guard&&) = delete;
    auto operator=(countdown_guard const&) -> countdown_guard& = delete;
    auto operator=(countdown_guard&&) -> countdown_guard& = delete;
    ~countdown_guard() {
        throws_on_copy::copies_until_throw = 0;
    }
};

template <typename Map>
void check_throwing_copy_assignment(int source_size, int target_size, int copies_until_throw) {
    auto source = test::filled<Map>(source_size);
    auto target = test::filled<Map>(target_size);

    {
        auto const guard = countdown_guard(copies_until_throw);
        REQUIRE_THROWS_AS(target = source, std::runtime_error);
    }

    test::require_empty_table_answers(target);
    // The source is untouched -- it was only ever read from.
    REQUIRE(source.size() == static_cast<std::size_t>(source_size));
}

} // namespace

TEST_CASE("copy_assignment_that_throws_leaves_a_usable_table") {
    SUBCASE("map") {
        check_throwing_copy_assignment<map_t>(200, 50, 100);
    }

    SUBCASE("segmented map") {
        check_throwing_copy_assignment<segmented_map_t>(200, 50, 100);
    }

    // Throwing on the very first copy, before the values container has committed to anything.
    SUBCASE("on the first copy") {
        check_throwing_copy_assignment<map_t>(200, 50, 1);
    }

    // Assigning onto a table that has no buckets to give back in the first place.
    SUBCASE("onto an empty table") {
        check_throwing_copy_assignment<map_t>(200, 0, 100);
    }
}

// Move assignment has the identical window and its own recovery, and nothing was reaching it: both
// the reset_to_empty() that puts the table back and the bare `throw;` that re-raises could be
// deleted with the suite green.
//
// It takes more setting up than the copy above, because a move only reaches the recovery when it
// can fail at all. With std::allocator the whole operation is noexcept -- the containers simply
// take the other's memory -- so `move_assign_is_nothrow` is true, the try/catch is not even
// instantiated, and there is nothing to test. An allocator that neither propagates on move
// assignment nor compares equal is what makes the table move the elements one at a time into
// memory of its own, which is a loop with a throwing move in it.
namespace {

// Throws on the nth move. The mirror of throws_on_copy above, and it has to be copyable without
// throwing for the same reason that one has to be movable.
struct throws_on_move {
    static inline int moves_until_throw = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    int m_value = 0;

    throws_on_move() = default;

    explicit throws_on_move(int value)
        : m_value(value) {}

    throws_on_move(throws_on_move const&) = default;
    auto operator=(throws_on_move const&) -> throws_on_move& = default;
    ~throws_on_move() = default;

    throws_on_move(throws_on_move&& other)
        : m_value(other.m_value) {
        if (0 < moves_until_throw && 0 == --moves_until_throw) {
            throw std::runtime_error("move");
        }
    }

    auto operator=(throws_on_move&& other) -> throws_on_move& {
        m_value = other.m_value;
        if (0 < moves_until_throw && 0 == --moves_until_throw) {
            throw std::runtime_error("move");
        }
        return *this;
    }
};

using throwing_move_alloc = test::pmr_like_allocator<std::pair<int, throws_on_move>>;
using throwing_move_map =
    ankerl::unordered_dense::map<int, throws_on_move, ankerl::unordered_dense::hash<int>, std::equal_to<int>, throwing_move_alloc>;

// The whole point of the allocator choice, asserted rather than assumed: with a nothrow move
// assignment the recovery below is not instantiated and the test would pass vacuously.
static_assert(!std::is_nothrow_move_assignable_v<throwing_move_map>,
              "the recovery this file tests only exists when move assignment can throw");

struct move_countdown_guard {
    explicit move_countdown_guard(int n) {
        throws_on_move::moves_until_throw = n;
    }
    move_countdown_guard(move_countdown_guard const&) = delete;
    move_countdown_guard(move_countdown_guard&&) = delete;
    auto operator=(move_countdown_guard const&) -> move_countdown_guard& = delete;
    auto operator=(move_countdown_guard&&) -> move_countdown_guard& = delete;
    ~move_countdown_guard() {
        throws_on_move::moves_until_throw = 0;
    }
};

auto filled_with(int count, int allocator_id) -> throwing_move_map {
    auto map = throwing_move_map(0, throwing_move_alloc(allocator_id));
    for (int i = 0; i < count; ++i) {
        map.try_emplace(i, throws_on_move(i));
    }
    return map;
}

} // namespace

TEST_CASE("move_assignment_that_throws_leaves_a_usable_table") {
    auto source = filled_with(200, 1);
    auto target = filled_with(50, 2);

    // Unequal allocators are what stop the move being a pointer swap.
    REQUIRE(!(source.get_allocator() == target.get_allocator()));

    {
        auto const guard = move_countdown_guard(100);
        REQUIRE_THROWS_AS(target = std::move(source), std::runtime_error);
    }

    test::require_empty_table_answers(target);
}

// The recovery has one more corner, and it is the only one where the *bucket* array is what the
// table is left holding. copy_everything_from() copies the values first and builds the buckets
// last, so every test above throws while m_buckets is still empty from the deallocate_buckets()
// that ran before the try -- which makes reset_to_empty()'s m_buckets.clear() a no-op and leaves
// it untested.
//
// A failure inside the bucket build is what reaches it, and only for a segmented table: the
// default container builds a fresh array beside the old one and swaps it in, so a throw there
// leaves m_buckets exactly as it was, while the segmented one grows in place and a throw part way
// through leaves a partly grown array behind.
//
// Rather than guess which allocation that is -- it moves with the code, and MSVC's debug iterators
// take one through the allocator for every container constructed -- this walks the budget upward
// until the assignment completes, so every allocation it makes has had its turn at failing.
namespace {

template <typename Alloc>
using assign_map_of = ankerl::unordered_dense::map<int, int, ankerl::unordered_dense::hash<int>, std::equal_to<int>, Alloc>;

template <typename Alloc>
using assign_segmented_map_of =
    ankerl::unordered_dense::segmented_map<int, int, ankerl::unordered_dense::hash<int>, std::equal_to<int>, Alloc>;

using assign_pair_t = std::pair<int, int>;
using assign_bombing_map = assign_map_of<test::bombing_allocator<assign_pair_t>>;
using assign_bombing_segmented_map = assign_segmented_map_of<test::bombing_allocator<assign_pair_t>>;

#if ANKERL_TEST_CAN_BOMB_ALLOCATIONS()
template <typename Map>
void require_assignment_survives_every_allocation_failure() {
    auto failures = 0;
    auto completed = false;
    for (int budget = 0; budget < 1000 && !completed; ++budget) {
        auto const source = test::filled<Map>(200);
        auto target = test::filled<Map>(50);

        try {
            auto const bomb = test::bomb_after(budget);
            target = source;
            completed = true;
        } catch (std::bad_alloc const&) {
            ++failures;
            // Whichever allocation failed, the table is back to what a default constructed one
            // looks like -- including holding no bucket array, which is the part that a throw
            // during the bucket build is the only way to reach.
            test::require_empty_table_answers(target);
        }
    }
    // Both halves matter: with no failure the loop proved nothing, and with no completion it never
    // reached the end and the later allocations went untested.
    REQUIRE(failures > 0);
    REQUIRE(completed);
}
#endif

} // namespace

#if ANKERL_TEST_CAN_BOMB_ALLOCATIONS()

TEST_CASE("copy_assignment_that_cannot_allocate_leaves_a_usable_table") {
    SUBCASE("map") {
        require_assignment_survives_every_allocation_failure<assign_bombing_map>();
    }

    // The one that can throw with a partly built bucket array behind it.
    SUBCASE("segmented map") {
        require_assignment_survives_every_allocation_failure<assign_bombing_segmented_map>();
    }
}

#endif
