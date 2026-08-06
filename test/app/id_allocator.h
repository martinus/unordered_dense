#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>

// An allocator whose instances are distinguishable, for testing allocator propagation.
//
// std::allocator makes none of that observable: it is stateless, every instance compares equal and
// is_always_equal is true, so every branch that asks whether two allocators differ is dead code.
// This one carries an id, so taking the wrong allocator shows up as a value rather than as
// undefined behaviour, and optionally counts what was allocated through it, so it is also visible
// *which* of a container's buffers went where.
//
// The defaults match std::pmr::polymorphic_allocator, which is the allocator this library supports
// whose propagation is worth testing: propagates on nothing, instances differ.
namespace test {

struct alloc_counts {
    int allocations = 0;
    int deallocations = 0;
};

template <typename T, typename Pocca = std::false_type, typename Soccc = std::true_type>
struct id_allocator {
    using value_type = T;
    using propagate_on_container_copy_assignment = Pocca;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_swap = std::false_type;
    using is_always_equal = std::false_type;

    int m_id = 0;
    alloc_counts* m_counts = nullptr;

    id_allocator() = default;

    explicit id_allocator(int id, alloc_counts* counts = nullptr)
        : m_id(id)
        , m_counts(counts) {}

    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    id_allocator(id_allocator<U, Pocca, Soccc> const& other) noexcept
        : m_id(other.m_id)
        , m_counts(other.m_counts) {}

    auto allocate(std::size_t n) -> T* {
        if (nullptr != m_counts) {
            ++m_counts->allocations;
        }
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, std::size_t n) {
        if (nullptr != m_counts) {
            ++m_counts->deallocations;
        }
        std::allocator<T>{}.deallocate(p, n);
    }

    // Soccc == false_type behaves like std::pmr::polymorphic_allocator: a copy does not inherit
    // the allocator, it gets the default one. The true_type default is what allocator_traits does
    // on its own, i.e. hand back a copy.
    [[nodiscard]] auto select_on_container_copy_construction() const -> id_allocator {
        if constexpr (Soccc::value) {
            return *this;
        } else {
            return id_allocator{};
        }
    }

    friend auto operator==(id_allocator const& a, id_allocator const& b) noexcept -> bool {
        return a.m_id == b.m_id;
    }

    friend auto operator!=(id_allocator const& a, id_allocator const& b) noexcept -> bool {
        return !(a == b);
    }
};

} // namespace test
