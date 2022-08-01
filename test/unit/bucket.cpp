#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <limits>
#include <stdexcept> // for out_of_range

using Map = ankerl::unordered_dense::map<std::string, size_t>;

// big bucket type allows 2^64 elements, but has more memory & CPU overhead.
using MapBig = ankerl::unordered_dense::map<std::string,
                                            size_t,
                                            ankerl::unordered_dense::hash<std::string>,
                                            std::equal_to<std::string>,
                                            std::allocator<std::pair<std::string, size_t>>,
                                            ankerl::unordered_dense::bucket_type::big>;

static_assert(sizeof(Map::bucket_type) == 8U);
static_assert(sizeof(MapBig::bucket_type) == sizeof(size_t) + 4U);

static_assert(Map::max_size() == std::numeric_limits<uint32_t>::max());
static_assert(MapBig::max_size() == std::numeric_limits<size_t>::max());

static_assert(Map::max_bucket_count() == std::numeric_limits<uint32_t>::max());
static_assert(MapBig::max_bucket_count() == std::numeric_limits<size_t>::max());

TEST_CASE("bucket") {
    // TODO nothing here yet
}
