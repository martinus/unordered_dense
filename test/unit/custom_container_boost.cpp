#if 0

#    include <ankerl/unordered_dense.h>

#    include <boost/container/vector.hpp>
#    include <boost/interprocess/allocators/allocator.hpp>
#    include <boost/interprocess/allocators/node_allocator.hpp>
#    include <boost/interprocess/containers/vector.hpp>
#    include <boost/interprocess/managed_shared_memory.hpp>
#    include <doctest.h>

#    include <deque>

#    if 0
// See https://www.boost.org/doc/libs/1_80_0/doc/html/interprocess/allocators_containers.html
TEST_CASE("boost_container_vector") {
    // Remove shared memory on construction and destruction
    struct ShmRemove {
        ShmRemove() {
            boost::interprocess::shared_memory_object::remove("MySharedMemory");
        }
        ~ShmRemove() {
            boost::interprocess::shared_memory_object::remove("MySharedMemory");
        }
    } remover;

    // Create shared memory
    auto segment = boost::interprocess::managed_shared_memory(boost::interprocess::create_only, "MySharedMemory", 65536);

    // Alias an STL-like allocator of ints that allocates ints from the segment
    using ShmemAllocator = boost::interprocess::allocator<std::pair<int, std::string>,
                                                          boost::interprocess::managed_shared_memory::segment_manager>;
    using ShmemVector = boost::interprocess::vector<std::pair<int, std::string>, ShmemAllocator>;

    using Map =
        ankerl::unordered_dense::map<int, std::string, ankerl::unordered_dense::hash<int>, std::equal_to<int>, ShmemVector>;

    auto map = Map{ShmemVector{ShmemAllocator{segment.get_segment_manager()}}};

    for (int i = 0; i < 100; ++i) {
        map.try_emplace(i, std::to_string(i));
    }

    REQUIRE(map.size() == 100);
    for (int i = 0; i < 100; ++i) {
        auto it = map.find(i);
        REQUIRE(it != map.end());
        REQUIRE(it->first == i);
        REQUIRE(it->second == std::to_string(i));
    }

    map.erase(123);
    REQUIRE(map.size() == 100);
    map.erase(29);
    REQUIRE(map.size() == 99);

    map.emplace(std::pair<int, std::string>(9999, "hello"));
    REQUIRE(map.size() == 100);
}
#    endif

#endif