#if ANKERL_UNORDERED_DENSE_HAS_BOOST

#    include <ankerl/unordered_dense.h>

#    include <boost/container/vector.hpp>
#    include <boost/interprocess/allocators/allocator.hpp>
#    include <boost/interprocess/allocators/node_allocator.hpp>
#    include <boost/interprocess/containers/vector.hpp>
#    include <boost/interprocess/managed_shared_memory.hpp>
#    include <doctest.h>

#    include <deque>

// See https://www.boost.org/doc/libs/1_80_0/doc/html/interprocess/allocators_containers.html
TEST_CASE("boost_container_vector") {
    // Remove shared memory on construction and destruction
    struct shm_remove {
        shm_remove() {
            boost::interprocess::shared_memory_object::remove("MySharedMemory");
        }
        ~shm_remove() {
            boost::interprocess::shared_memory_object::remove("MySharedMemory");
        }

        shm_remove(shm_remove const&) = delete;
        shm_remove(shm_remove&&) = delete;
        auto operator=(shm_remove const&) -> shm_remove = delete;
        auto operator=(shm_remove&&) -> shm_remove = delete;
    };

    auto remover = shm_remove();

    // Create shared memory
    auto segment = boost::interprocess::managed_shared_memory(boost::interprocess::create_only, "MySharedMemory", 65536);

    // Alias an STL-like allocator of ints that allocates ints from the segment
    using shmem_allocator = boost::interprocess::allocator<std::pair<int, std::string>,
                                                           boost::interprocess::managed_shared_memory::segment_manager>;
    using shmem_vector = boost::interprocess::vector<std::pair<int, std::string>, shmem_allocator>;

    using map_t =
        ankerl::unordered_dense::map<int, std::string, ankerl::unordered_dense::hash<int>, std::equal_to<int>, shmem_vector>;

    auto map = map_t{shmem_allocator{segment.get_segment_manager()}};

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
#endif