#include <ankerl/huge_page_allocator.h>
#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstddef> // for size_t
#include <cstdint> // for uint64_t, uintptr_t
#include <functional>
#include <memory> // for allocator_traits
#include <type_traits>
#include <utility> // for pair, move

// The allocator itself, then the map on top of it over every container shape. The map part is what
// #231 asked for -- that it round-trips -- and the shapes are not decoration: segmented_vector asks
// for 64 KB segments that stay below the threshold while its index goes above it, so that one map
// exercises both paths at once.
namespace {

template <class T>
using huge = ankerl::unordered_dense::huge_page_allocator<T>;

using traits = std::allocator_traits<huge<int>>;
static_assert(traits::is_always_equal::value);
static_assert(traits::propagate_on_container_move_assignment::value);
static_assert(std::is_same_v<traits::rebind_alloc<double>, huge<double>>);
static_assert(huge<int>::threshold == (std::size_t{2} << 20U));
static_assert(huge<int>::is_huge(huge<int>::threshold / sizeof(int)) == huge<int>::uses_huge_pages);
static_assert(!huge<int>::is_huge(huge<int>::threshold / sizeof(int) - 1));

} // namespace

TEST_CASE("huge_page_allocator_large_blocks_are_huge_page_aligned") {
    auto alloc = huge<std::uint64_t>();
    constexpr auto n = std::size_t{3} << 18U; // 2 MB of uint64_t: exactly the threshold, rounded to one page
    auto* p = alloc.allocate(n);
    REQUIRE(p != nullptr);
    if constexpr (huge<std::uint64_t>::uses_huge_pages) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        REQUIRE(reinterpret_cast<std::uintptr_t>(p) % huge<std::uint64_t>::huge_page_size == 0);
    }
    // every 4 KB page of it is writable and reads back
    for (std::size_t i = 0; i < n; i += 512) {
        p[i] = i;
    }
    for (std::size_t i = 0; i < n; i += 512) {
        REQUIRE(p[i] == i);
    }
    alloc.deallocate(p, n);

    // below the threshold it is std::allocator's memory, with no alignment promise beyond the type's
    auto* q = alloc.allocate(16);
    q[0] = 1;
    q[15] = 2;
    REQUIRE(q[0] + q[15] == 3);
    alloc.deallocate(q, 16);
}

TEST_CASE("huge_page_allocator_instances_compare_equal_across_rebinds") {
    REQUIRE(huge<int>() == huge<double>());
    REQUIRE(!(huge<int>() != huge<char>()));
    auto a = huge<int>();
    auto b = huge<double>(a); // converting constructor, the rebind path a container takes
    REQUIRE(a == b);
    // a different threshold is a different allocator
    REQUIRE(huge<int>() != ankerl::unordered_dense::huge_page_allocator<int, (std::size_t{4} << 20U)>());
}

TEST_CASE_MAP("huge_page_allocator_round_trips_with_the_map",
              std::uint64_t,
              std::uint64_t,
              ankerl::unordered_dense::hash<std::uint64_t>,
              std::equal_to<std::uint64_t>,
              huge<std::pair<std::uint64_t, std::uint64_t>>) {
    // 300000 entries: the values are 4.8 MB and the index 2.9 MB (or 5 MB for group_big), so both
    // regions are above the threshold and come from mmap. Every erase below then shrinks nothing,
    // and the rehash at the end reallocates the index through the same path.
    constexpr auto n = std::uint64_t{300000};
    auto map = map_t();
    for (std::uint64_t i = 0; i < n; ++i) {
        map[i] = i * 2;
    }
    REQUIRE(map.size() == n);
    for (std::uint64_t i = 0; i < n; i += 7) {
        auto it = map.find(i);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i * 2);
    }
    for (std::uint64_t i = 0; i < n; i += 2) {
        REQUIRE(map.erase(i) == 1);
    }
    REQUIRE(map.size() == n / 2);
    REQUIRE(!map.contains(0));
    REQUIRE(map.contains(1));

    auto copy = map;
    REQUIRE(copy.get_allocator() == map.get_allocator());
    REQUIRE(copy == map);

    auto moved = std::move(map);
    REQUIRE(moved.size() == n / 2);
    REQUIRE(moved == copy);

    moved.rehash(0);
    REQUIRE(moved == copy);
    moved.swap(copy);
    copy.clear();
    REQUIRE(copy.empty());
    REQUIRE(moved.size() == n / 2);
    REQUIRE(moved.find(12345)->second == 12345 * 2);
}

// The aliases exist so that opting in is one word instead of five template arguments, so what there
// is to check is that the word names exactly the type the five arguments do. Nothing else: the
// assert is type identity, and a map of the same type cannot behave differently.
TEST_CASE("huge_page_aliases_name_the_same_types_as_the_arguments_they_replace") {
    namespace udm = ankerl::unordered_dense;

    static_assert(std::is_same_v<udm::huge_page::map<int, int>,
                                 udm::map<int, int, udm::hash<int>, std::equal_to<int>, huge<std::pair<int, int>>>>);
    static_assert(std::is_same_v<udm::huge_page::segmented_map<int, int>,
                                 udm::segmented_map<int, int, udm::hash<int>, std::equal_to<int>, huge<std::pair<int, int>>>>);
    static_assert(std::is_same_v<udm::huge_page::set<int>, udm::set<int, udm::hash<int>, std::equal_to<int>, huge<int>>>);
    static_assert(std::is_same_v<udm::huge_page::segmented_set<int>,
                                 udm::segmented_set<int, udm::hash<int>, std::equal_to<int>, huge<int>>>);

    // Every parameter the alias forwards, forwarded: an alias that dropped one and hardcoded its
    // default would satisfy all four asserts above.
    static_assert(std::is_same_v<udm::huge_page::map<int, int, std::hash<int>>,
                                 udm::map<int, int, std::hash<int>, std::equal_to<int>, huge<std::pair<int, int>>>>);
    static_assert(
        std::is_same_v<
            udm::huge_page::map<int, int, udm::hash<int>, std::equal_to<int>, udm::bucket_type::group_big>,
            udm::map<int, int, udm::hash<int>, std::equal_to<int>, huge<std::pair<int, int>>, udm::bucket_type::group_big>>);
}
