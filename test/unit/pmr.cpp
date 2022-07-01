#include <ankerl/unordered_dense.h>

#include <doctest.h>
#include <fmt/format.h>

#include <memory_resource>

class logging_memory_resource : public std::pmr::memory_resource {
    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
        fmt::print("+ {} bytes, {} alignment\n", bytes, alignment);
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        fmt::print("- {} bytes, {} alignment, {} ptr\n", bytes, alignment, p);
        return std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
    }

    [[nodiscard]] auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override {
        return this == &other;
    }
};

class track_peak_memory_resource : public std::pmr::memory_resource {
    uint64_t m_peak = 0;
    uint64_t m_current = 0;

    auto do_allocate(std::size_t bytes, std::size_t alignment) -> void* override {
        m_current += bytes;
        if (m_current > m_peak) {
            m_peak = m_current;
        }
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        m_current -= bytes;
        return std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
    }

    [[nodiscard]] auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override {
        return this == &other;
    }

public:
    [[nodiscard]] auto current() const -> uint64_t {
        return m_current;
    }

    [[nodiscard]] auto peak() const -> uint64_t {
        return m_peak;
    }
};

TEST_CASE("pmr") {
    auto mr = track_peak_memory_resource();
    {
        REQUIRE(mr.current() == 0);
        auto map = ankerl::unordered_dense::pmr::map<uint64_t, uint64_t>(&mr);
        REQUIRE(mr.current() == 0);

        for (size_t i = 0; i < 1; ++i) {
            map[i] = i;
        }
        REQUIRE(mr.current() != 0);

        // gets a copy, but it has the same memory resource
        auto alloc = map.get_allocator();
        REQUIRE(alloc.resource() == &mr);
    }
    REQUIRE(mr.current() == 0);
}
