#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <memory>
#include <string>
#include <type_traits>
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
template <typename T>
struct sticky_allocator {
    using value_type = T;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;
    using is_always_equal = std::false_type;

    int m_id = 0;

    sticky_allocator() = default;
    explicit sticky_allocator(int id)
        : m_id(id) {}

    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    sticky_allocator(sticky_allocator<U> const& other) noexcept
        : m_id(other.m_id) {}

    auto allocate(std::size_t n) -> T* {
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, std::size_t n) {
        std::allocator<T>{}.deallocate(p, n);
    }

    friend auto operator==(sticky_allocator const& a, sticky_allocator const& b) noexcept -> bool {
        return a.m_id == b.m_id;
    }

    friend auto operator!=(sticky_allocator const& a, sticky_allocator const& b) noexcept -> bool {
        return !(a == b);
    }
};

// The same, except that it propagates on copy assignment, so the POCCA branch is reachable.
template <typename T>
struct copying_allocator : sticky_allocator<T> {
    using propagate_on_container_copy_assignment = std::true_type;

    using sticky_allocator<T>::sticky_allocator;

    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    copying_allocator(copying_allocator<U> const& other) noexcept
        : sticky_allocator<T>(other.m_id) {}
};

template <typename Alloc>
using vec_of = ankerl::unordered_dense::segmented_vector<int, Alloc, sizeof(int) * 4>;

using sticky_vec = vec_of<sticky_allocator<int>>;
using copying_vec = vec_of<copying_allocator<int>>;
using std_vec = vec_of<std::allocator<int>>;

template <typename Vec>
auto filled(typename Vec::allocator_type alloc, int count) -> Vec {
    auto vec = Vec(alloc);
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

} // namespace

// Move assignment between allocators that neither propagate nor compare equal has to move the
// elements one at a time, and that allocates. It was noexcept anyway, so running out of memory
// there terminated the process instead of throwing.
TEST_CASE("segmented_vector_move_assign_noexcept_only_when_it_cannot_throw") {
    static_assert(std::is_nothrow_move_assignable_v<std_vec>);
    static_assert(std::is_nothrow_move_assignable_v<ankerl::unordered_dense::segmented_vector<std::string>>);
    static_assert(!std::is_nothrow_move_assignable_v<sticky_vec>);

    // ... and the container built on it says the same thing, since that is what callers see.
    using sticky_map = ankerl::unordered_dense::
        segmented_map<int, int, ankerl::unordered_dense::hash<int>, std::equal_to<int>, sticky_allocator<std::pair<int, int>>>;
    static_assert(!std::is_nothrow_move_assignable_v<sticky_map>);
    static_assert(std::is_nothrow_move_assignable_v<ankerl::unordered_dense::segmented_map<int, int>>);
}

TEST_CASE("segmented_vector_move_assign_keeps_its_own_allocator") {
    auto source = filled<sticky_vec>(sticky_allocator<int>(1), 10);
    auto target = filled<sticky_vec>(sticky_allocator<int>(2), 3);

    target = std::move(source);

    // Nothing propagates, so the target keeps the allocator it was built with. It used to adopt
    // the source's, which meant memory allocated from one arena was later freed through another.
    REQUIRE(target.get_allocator().m_id == 2);
    require_holds(target, 10);
}

TEST_CASE("segmented_vector_move_assign_between_equal_allocators_steals") {
    auto source = filled<sticky_vec>(sticky_allocator<int>(7), 10);
    auto target = filled<sticky_vec>(sticky_allocator<int>(7), 3);

    target = std::move(source);

    REQUIRE(target.get_allocator().m_id == 7);
    require_holds(target, 10);
    REQUIRE(source.empty()); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
}

TEST_CASE("segmented_vector_copy_construction_asks_the_allocator") {
    auto source = filled<sticky_vec>(sticky_allocator<int>(5), 10);

    auto copy = source;

    // select_on_container_copy_construction defaults to handing back a copy, so this one inherits
    // the id. The copy constructor used to default construct its allocator and lose it.
    REQUIRE(copy.get_allocator().m_id == 5);
    require_holds(copy, 10);
    require_holds(source, 10);
}

TEST_CASE("segmented_vector_copy_construction_with_an_explicit_allocator") {
    auto source = filled<sticky_vec>(sticky_allocator<int>(5), 10);

    auto copy = sticky_vec(source, sticky_allocator<int>(9));

    REQUIRE(copy.get_allocator().m_id == 9);
    require_holds(copy, 10);
}

TEST_CASE("segmented_vector_copy_assign_propagates_only_when_asked") {
    SUBCASE("without pocca the target keeps its allocator") {
        auto source = filled<sticky_vec>(sticky_allocator<int>(1), 10);
        auto target = filled<sticky_vec>(sticky_allocator<int>(2), 3);

        target = source;

        REQUIRE(target.get_allocator().m_id == 2);
        require_holds(target, 10);
    }

    SUBCASE("with pocca it takes the source's") {
        auto source = filled<copying_vec>(copying_allocator<int>(1), 10);
        auto target = filled<copying_vec>(copying_allocator<int>(2), 3);

        target = source;

        REQUIRE(target.get_allocator().m_id == 1);
        require_holds(target, 10);
    }
}

TEST_CASE("segmented_vector_self_assignment_keeps_the_contents") {
    auto vec = filled<sticky_vec>(sticky_allocator<int>(3), 10);

    auto& alias = vec;
    vec = alias;
    require_holds(vec, 10);
    REQUIRE(vec.get_allocator().m_id == 3);
}

// The move constructor reads other's allocator to build itself with. It used to read its own,
// before its own existed; that has since been fixed, and this keeps it fixed.
TEST_CASE("segmented_vector_move_construction_takes_the_source_allocator") {
    auto source = filled<sticky_vec>(sticky_allocator<int>(4), 10);

    auto moved = std::move(source);

    REQUIRE(moved.get_allocator().m_id == 4);
    require_holds(moved, 10);
}
