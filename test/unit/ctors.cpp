#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <app/counter.h>

#include <cstddef> // for size_t
#include <utility> // for pair

// very minimal input iterator
// https://en.cppreference.com/w/cpp/named_req/InputIterator#Concept
class it {
    std::pair<counter::obj, counter::obj> m_kv;

public:
    it(size_t val, counter& counts)
        : m_kv({val, counts}, {val, counts}) {}

    auto operator++() -> it& {
        ++m_kv.first.get();
        ++m_kv.second.get();
        return *this;
    }

    auto operator*() -> std::pair<counter::obj, counter::obj> const& {
        return m_kv;
    }

    auto operator!=(it const& other) const -> bool {
        return other.m_kv.first.get() != m_kv.first.get() || other.m_kv.second.get() != m_kv.second.get();
    }
};

TEST_CASE("ctors_map") {
    using map_t = ankerl::unordered_dense::map<counter::obj, counter::obj>;
    // using map_t = std::unordered_map<counter::obj, counter::obj>;
    using alloc_t = map_t::allocator_type;
    using hash_t = map_t::hasher;
    using key_eq_t = map_t::key_equal;

    auto counts = counter();
    INFO(counts);

    { auto m = map_t{}; }
    { auto m = map_t{0, alloc_t{}}; }
    { auto m = map_t{0, hash_t{}, alloc_t{}}; }
    { auto m = map_t{alloc_t{}}; }
    REQUIRE(counts.dtor() == 0);

    {
        auto begin_it = it{size_t{0}, counts};
        auto end_it = it{size_t{10}, counts};
        auto m = map_t{begin_it, end_it};
        REQUIRE(m.size() == 10);
    }
    {
        auto begin_it = it{size_t{0}, counts};
        auto end_it = it{size_t{10}, counts};
        auto m = map_t{begin_it, end_it, 0, alloc_t{}};
        REQUIRE(m.size() == 10);
    }
    {
        auto begin_it = it{size_t{0}, counts};
        auto end_it = it{size_t{10}, counts};
        auto m = map_t{begin_it, end_it, 0, hash_t{}, alloc_t{}};
        REQUIRE(m.size() == 10);
    }
    {
        auto begin_it = it{size_t{0}, counts};
        auto end_it = it{size_t{10}, counts};
        auto m = map_t{begin_it, end_it, 0, hash_t{}, key_eq_t{}};
        REQUIRE(m.size() == 10);
    }
}

TEST_CASE("ctor_bucket_count") {
    {
        auto m = ankerl::unordered_dense::map<counter::obj, counter::obj>{};
        REQUIRE(m.bucket_count() == 0U);
    }
    {
        auto m = ankerl::unordered_dense::map<counter::obj, counter::obj>{150U};
        REQUIRE(m.bucket_count() == 256U);
    }
    {
        auto m = ankerl::unordered_dense::set<int>{{1, 2, 3, 4}, 300U};
        REQUIRE(m.size() == 4U);
        REQUIRE(m.bucket_count() == 512U);
    }
}
