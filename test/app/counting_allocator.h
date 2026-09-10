#pragma once

#include <cstddef>
#include <memory>
#include <vector>

// How many times a container asked its allocator for memory, or handed it back.
//
// This is a *count*, for tests that assert a container allocates once per segment or not at all on
// a swap. It is deliberately not a memory measurement: what a request really costs is what the
// allocator rounded it up to, which only the allocator can say, and a container's own allocator
// never sees what a value type allocates for itself. scripts/ab/alloc_timeline.cpp measures memory,
// by replacing global operator new and asking malloc_usable_size.
class counts_for_allocator {
    std::vector<std::size_t> m_measurements{};

public:
    void add(size_t count) {
        m_measurements.emplace_back(count);
    }

    void sub(size_t count) {
        // overflow, but it's ok
        m_measurements.emplace_back(0U - count);
    }

    [[nodiscard]] auto size() const -> size_t {
        return m_measurements.size();
    }

    void reset() {
        m_measurements.clear();
    }
};

/**
 * Forwards all allocations/deallocations to the counts
 */
template <class T>
class counting_allocator {
    counts_for_allocator* m_counts;

    template <typename U>
    friend class counting_allocator;

public:
    using value_type = T;

    /**
     * Not explicit so we can easily construct it with the correct resource
     */
    counting_allocator(counts_for_allocator* counts) noexcept // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
        : m_counts(counts) {}

    /**
     * Not explicit so we can easily construct it with the correct resource
     */
    template <class U>
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    counting_allocator(counting_allocator<U> const& other) noexcept
        : m_counts(other.m_counts) {}

    counting_allocator(counting_allocator const& other) noexcept = default;
    counting_allocator(counting_allocator&& other) noexcept = default;
    auto operator=(counting_allocator const& other) noexcept -> counting_allocator& = default;
    auto operator=(counting_allocator&& other) noexcept -> counting_allocator& = default;
    ~counting_allocator() = default;

    auto allocate(size_t n) -> T* {
        m_counts->add(sizeof(T) * n);
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, size_t n) noexcept {
        m_counts->sub(sizeof(T) * n);
        std::allocator<T>{}.deallocate(p, n);
    }

    template <class U>
    friend auto operator==(counting_allocator const& a, counting_allocator<U> const& b) noexcept -> bool {
        return a.m_counts == b.m_counts;
    }

    template <class U>
    friend auto operator!=(counting_allocator const& a, counting_allocator<U> const& b) noexcept -> bool {
        return a.m_counts != b.m_counts;
    }
};
