#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/id_allocator.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Allocator propagation in segmented_vector, from issue #104.
//
// std::allocator makes none of this observable: it is stateless, every instance compares equal, and
// is_always_equal is true, so every branch that depends on "are these two allocators the same" is
// dead. These two carry an id instead, so taking the wrong one shows up as a value rather than as
// undefined behaviour.
namespace {

// Propagates on nothing and instances differ, which is how std::pmr::polymorphic_allocator behaves
// -- the allocator this library supports and tests, and the reason issue #104's proposed
// static_assert(pocma) fix cannot be applied here.
template <typename Alloc>
using vec_of = ankerl::unordered_dense::segmented_vector<int, Alloc, sizeof(int) * 4>;

using sticky_vec = vec_of<test::id_allocator<int>>;
using copying_vec = vec_of<test::pocca_allocator<int>>;
using std_vec = vec_of<std::allocator<int>>;

template <typename Vec>
auto filled(int allocator_id, int count) -> Vec {
    auto vec = Vec(typename Vec::allocator_type(allocator_id));
    for (int i = 0; i < count; ++i) {
        vec.emplace_back(i);
    }
    return vec;
}

template <typename Vec>
void require_holds(Vec const& vec, int count) {
    REQUIRE(vec.size() == static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        REQUIRE(vec[static_cast<std::size_t>(i)] == i);
    }
}

template <typename Vec>
void check_copy_assign(int expected_allocator_id) {
    auto source = filled<Vec>(1, 10);
    auto target = filled<Vec>(2, 3);

    target = source;

    REQUIRE(target.get_allocator().m_id == expected_allocator_id);
    require_holds(target, 10);
}

} // namespace

// Move assignment between allocators that neither propagate nor compare equal moves the elements
// one at a time, and that allocates. It was noexcept anyway, so running out of memory there
// terminated the process instead of throwing.
static_assert(std::is_nothrow_move_assignable_v<std_vec>);
static_assert(std::is_nothrow_move_assignable_v<ankerl::unordered_dense::segmented_vector<std::string>>);
static_assert(!std::is_nothrow_move_assignable_v<sticky_vec>);

// ... and the container built on it says the same thing, since that is what callers see.
using sticky_map = ankerl::unordered_dense::
    segmented_map<int, int, ankerl::unordered_dense::hash<int>, std::equal_to<int>, test::id_allocator<std::pair<int, int>>>;
static_assert(!std::is_nothrow_move_assignable_v<sticky_map>);
static_assert(std::is_nothrow_move_assignable_v<ankerl::unordered_dense::segmented_map<int, int>>);

TEST_CASE("segmented_vector_move_assign_keeps_its_own_allocator") {
    auto source = filled<sticky_vec>(1, 10);
    auto target = filled<sticky_vec>(2, 3);

    target = std::move(source);

    // Nothing propagates, so the target keeps the allocator it was built with. It used to adopt
    // the source's, which meant memory allocated from one arena was later freed through another.
    REQUIRE(target.get_allocator().m_id == 2);
    require_holds(target, 10);
}

TEST_CASE("segmented_vector_move_assign_between_equal_allocators_steals") {
    auto source = filled<sticky_vec>(7, 10);
    auto target = filled<sticky_vec>(7, 3);

    target = std::move(source);

    REQUIRE(target.get_allocator().m_id == 7);
    require_holds(target, 10);
    REQUIRE(source.empty()); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
}

TEST_CASE("segmented_vector_copy_construction_asks_the_allocator") {
    auto source = filled<sticky_vec>(5, 10);

    auto copy = source;

    // select_on_container_copy_construction defaults to handing back a copy, so this one inherits
    // the id. The copy constructor used to default construct its allocator and lose it.
    REQUIRE(copy.get_allocator().m_id == 5);
    require_holds(copy, 10);
    require_holds(source, 10);
}

TEST_CASE("segmented_vector_copy_construction_with_an_explicit_allocator") {
    auto source = filled<sticky_vec>(5, 10);

    auto copy = sticky_vec(source, test::id_allocator<int>(9));

    REQUIRE(copy.get_allocator().m_id == 9);
    require_holds(copy, 10);
}

TEST_CASE("segmented_vector_copy_assign_propagates_only_when_asked") {
    // Named source, and not a temporary: assigning a prvalue would pick move assignment and stop
    // testing what the case is called.
    SUBCASE("without pocca the target keeps its allocator") {
        check_copy_assign<sticky_vec>(2);
    }

    SUBCASE("with pocca it takes the source's") {
        check_copy_assign<copying_vec>(1);
    }
}

TEST_CASE("segmented_vector_self_assignment_keeps_the_contents") {
    auto vec = filled<sticky_vec>(3, 10);

    auto& alias = vec;
    vec = alias;
    require_holds(vec, 10);
    REQUIRE(vec.get_allocator().m_id == 3);
}

// Pins a fix that predates this change: the move constructor builds itself from other's allocator.
TEST_CASE("segmented_vector_move_construction_takes_the_source_allocator") {
    auto source = filled<sticky_vec>(4, 10);

    auto moved = std::move(source);

    REQUIRE(moved.get_allocator().m_id == 4);
    require_holds(moved, 10);
}
