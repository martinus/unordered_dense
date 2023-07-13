#include <ankerl/unordered_dense.h>

#define ENABLE_LOG_LINE
#include <app/doctest.h>
#include <app/print.h>

#if defined(ANKERL_UNORDERED_DENSE_PMR)
// windows' vector has different allocation behavior, macos has linker errors
#    if __linux__

using int_str_map = ankerl::unordered_dense::pmr::map<int, std::string>;

// creates a map and moves it out
auto return_hello_world(ANKERL_UNORDERED_DENSE_PMR::memory_resource* resource) {
    int_str_map map_default_resource(resource);
    map_default_resource[0] = "Hello";
    map_default_resource[1] = "World";
    return map_default_resource;
}

TEST_CASE("move_with_allocators") {
    int_str_map map_default_resource(ANKERL_UNORDERED_DENSE_PMR::new_delete_resource());

    ANKERL_UNORDERED_DENSE_PMR::synchronized_pool_resource pool;

    // segfaults if m_buckets is reused
    {
        map_default_resource = return_hello_world(&pool);
        REQUIRE(map_default_resource.contains(0));
    }
}

#    endif
#endif