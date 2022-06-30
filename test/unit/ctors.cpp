#include <ankerl/unordered_dense.h>

#include <doctest.h>

TEST_CASE("ctors_map") {
    using Map = ankerl::unordered_dense::map<uint64_t, uint64_t>;
    using Alloc = Map::allocator_type;

    { auto m = Map{}; }

    { auto m = Map{0, Alloc{}}; }
}
