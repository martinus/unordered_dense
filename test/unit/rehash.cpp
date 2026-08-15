#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstddef> // for size_t
#include <cstdint> // for uint32_t
#include <new>     // for bad_alloc

TEST_CASE_MAP("rehash", size_t, int) {
    auto map = map_t();

    for (size_t i = 0; i < 1000; ++i) {
        map[i];
    }
    auto old_bucket_size = map.bucket_count();

    map.rehash(10000);
    REQUIRE(map.bucket_count() >= 10000);
    map.rehash(0);
    REQUIRE(map.bucket_count() == old_bucket_size);

    map.clear();
    map.rehash(0);
    REQUIRE(map.bucket_count() > 0);
    REQUIRE(map.bucket_count() < old_bucket_size);
}

// rehash() with a count above what the bucket array can ever hold. calc_shifts_for_size() walks the
// shift down until the capacity it computes covers the count -- but the bucket count saturates at
// max_bucket_count(), so above max_bucket_count() * max_load_factor() the capacity stops growing
// while the walk carries on, all the way to a shift of zero. That asks for `1 << 64`, which is
// undefined and in practice one: a table sized for billions came back with a single bucket and a
// mask of zero, and the next probe read past the end of it.
//
// rehash() is the reachable way in because, unlike reserve(), it does not size the value container
// first -- so there is no enormous allocation to fail before the bucket arithmetic runs.
TEST_CASE("rehash_above_the_bucket_limit_does_not_collapse_the_array") {
    using map_t = ankerl::unordered_dense::map<uint32_t, uint32_t>;
    auto map = map_t();
    map[1] = 1;

    auto const above_the_limit =
        static_cast<size_t>(static_cast<double>(map_t::max_bucket_count()) * map.max_load_factor()) + 1;

    // The array it ends up with may be as large as the machine allows, so this is allowed to fail
    // to allocate -- what it may not do is come back with an array too small to index.
    try {
        map.rehash(above_the_limit);
    } catch (std::bad_alloc const&) {
        return;
    }

    REQUIRE(map.bucket_count() == map_t::max_bucket_count());
    REQUIRE(map.size() == 1U);
    REQUIRE(map.find(1) != map.end());
    map[2] = 2;
    REQUIRE(map.find(2) != map.end());
    REQUIRE(map.size() == 2U);
}
