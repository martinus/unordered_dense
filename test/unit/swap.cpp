#include <ankerl/unordered_dense.h>

#define ENABLE_LOG_LINE
#include <app/doctest.h>
#include <app/print.h>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

TEST_CASE_MAP("swap", int, int) {
    {
        auto b = map_t();
        {
            auto a = map_t();
            b[1] = 2;

            a.swap(b);
            REQUIRE(a.end() != a.find(1));
            REQUIRE(b.end() == b.find(1));
        }
        REQUIRE(b.end() == b.find(1));
        b[2] = 3;
        REQUIRE(b.end() != b.find(2));
        REQUIRE(b.size() == 1);
    }

    {
        auto a = map_t();
        {
            auto b = map_t();
            b[1] = 2;

            a.swap(b);
            REQUIRE(a.end() != a.find(1));
            REQUIRE(b.end() == b.find(1));
        }
        REQUIRE(a.end() != a.find(1));
        a[2] = 3;
        REQUIRE(a.end() != a.find(2));
        REQUIRE(a.size() == 2);
    }

    {
        auto a = map_t();
        {
            auto b = map_t();
            a.swap(b);
            REQUIRE(a.end() == a.find(1));
            REQUIRE(b.end() == b.find(1));
        }
        REQUIRE(a.end() == a.find(1));
    }
}

namespace {

size_t g_num_allocations = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Stateless, so it stays default constructible and every instance compares equal: this counts allocations without
// changing anything else about how the map behaves.
template <typename T>
struct allocation_counting_allocator {
    using value_type = T;

    allocation_counting_allocator() = default;

    template <typename U>
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    allocation_counting_allocator(allocation_counting_allocator<U> const& /*other*/) noexcept {}

    auto allocate(size_t n) -> T* {
        ++g_num_allocations;
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, size_t n) noexcept {
        std::allocator<T>{}.deallocate(p, n);
    }
};

template <typename T, typename U>
auto operator==(allocation_counting_allocator<T> const& /*a*/, allocation_counting_allocator<U> const& /*b*/) noexcept
    -> bool {
    return true;
}

template <typename T, typename U>
auto operator!=(allocation_counting_allocator<T> const& /*a*/, allocation_counting_allocator<U> const& /*b*/) noexcept
    -> bool {
    return false;
}

} // namespace

// swap() went through the generic std::swap, so it move-assigned three times and each of those handed the moved-from
// map a fresh set of buckets. Exchanging what two maps already own needs no allocation at all.
TEST_CASE("swap_does_not_allocate") {
    using map_t = ankerl::unordered_dense::map<int,
                                               int,
                                               ankerl::unordered_dense::hash<int>,
                                               std::equal_to<int>,
                                               allocation_counting_allocator<std::pair<int, int>>>;

    auto a = map_t();
    auto b = map_t();
    for (int i = 0; i < 1000; ++i) {
        a[i] = i;
    }
    for (int i = 0; i < 10; ++i) {
        b[i] = -i;
    }

    g_num_allocations = 0;
    a.swap(b);
    REQUIRE(g_num_allocations == 0);

    REQUIRE(a.size() == 10U);
    REQUIRE(b.size() == 1000U);
    REQUIRE(a.at(5) == -5);
    REQUIRE(b.at(5) == 5);

    // and back again through the ADL form generic code uses, which now finds the member
    using std::swap;
    swap(a, b);
    REQUIRE(g_num_allocations == 0);
    REQUIRE(a.size() == 1000U);
    REQUIRE(b.size() == 10U);
    REQUIRE(a.at(999) == 999);
}
