#pragma once

#include <cstddef>
#include <memory>
#include <new>

// An allocator that throws on the nth allocation, for testing what a container does when it runs
// out of memory part way through an operation. The countdown is global rather than per instance,
// because the point is usually to fail one specific allocation inside a call, and a container's
// allocator has usually been copied and rebound several times by the time it gets there.
//
// Usable only where a container construction does not allocate. MSVC's debug iterators allocate a
// _Container_proxy through the allocator every time a container is constructed, and the standard
// declares std::vector's allocator-only constructor noexcept -- so a throwing allocator terminates
// there rather than propagating, whatever the container being tested does about it. That rules the
// whole technique out under _ITERATOR_DEBUG_LEVEL, which is what ANKERL_TEST_CAN_BOMB_ALLOCATIONS
// below reports.

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL != 0
#    define ANKERL_TEST_CAN_BOMB_ALLOCATIONS() 0 // NOLINT(cppcoreguidelines-macro-usage)
#else
#    define ANKERL_TEST_CAN_BOMB_ALLOCATIONS() 1 // NOLINT(cppcoreguidelines-macro-usage)
#endif

namespace test {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline int g_allocations_until_throw = -1;

// Arms the countdown for as long as it is in scope. -1, the resting state, never throws.
struct bomb_after {
    explicit bomb_after(int n) {
        g_allocations_until_throw = n;
    }
    bomb_after(bomb_after const&) = delete;
    bomb_after(bomb_after&&) = delete;
    auto operator=(bomb_after const&) -> bomb_after& = delete;
    auto operator=(bomb_after&&) -> bomb_after& = delete;
    ~bomb_after() {
        g_allocations_until_throw = -1;
    }
};

template <typename T>
struct bombing_allocator {
    using value_type = T;

    bombing_allocator() = default;

    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    bombing_allocator(bombing_allocator<U> const& /*unused*/) noexcept {}

    auto allocate(std::size_t n) -> T* {
        if (g_allocations_until_throw >= 0 && 0 == g_allocations_until_throw--) {
            throw std::bad_alloc{};
        }
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, std::size_t n) {
        std::allocator<T>{}.deallocate(p, n);
    }

    friend auto operator==(bombing_allocator /*unused*/, bombing_allocator /*unused*/) noexcept -> bool {
        return true;
    }

    friend auto operator!=(bombing_allocator /*unused*/, bombing_allocator /*unused*/) noexcept -> bool {
        return false;
    }
};

} // namespace test
