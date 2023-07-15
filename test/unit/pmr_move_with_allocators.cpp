#include <ankerl/unordered_dense.h>

#define ENABLE_LOG_LINE
#include <app/doctest.h>
#include <app/print.h>

#if defined(ANKERL_UNORDERED_DENSE_PMR)
// windows' vector has different allocation behavior, macos has linker errors
#    if __linux__

using int_str_map = ankerl::unordered_dense::pmr::map<int, std::string>;
using int_str_map_segmented = ankerl::unordered_dense::pmr::segmented_map<int, std::string>;

namespace {

// creates a map and moves it out
template <typename Map>
auto return_hello_world(ANKERL_UNORDERED_DENSE_PMR::memory_resource* resource) {
    Map map_default_resource(resource);
    map_default_resource[0] = "Hello";
    map_default_resource[1] = "World";
    return map_default_resource;
}

template <typename Map>
auto doTest() {
    Map map_default_resource(ANKERL_UNORDERED_DENSE_PMR::new_delete_resource());

    ANKERL_UNORDERED_DENSE_PMR::synchronized_pool_resource pool;

    // segfaults if m_buckets is reused
    {
        map_default_resource = return_hello_world<Map>(&pool);
        REQUIRE(map_default_resource.contains(0));
    }

    {
        Map map_default_resource_construct(return_hello_world<Map>(&pool), ANKERL_UNORDERED_DENSE_PMR::new_delete_resource());
        REQUIRE(map_default_resource.contains(0));
    }
}

TEST_CASE("move_with_allocators") {
    doTest<int_str_map>();
}

TEST_CASE("move_with_allocators_segmented") {
    doTest<int_str_map_segmented>();
}

} // namespace

#    endif
#endif