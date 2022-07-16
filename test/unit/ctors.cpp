#include <ankerl/unordered_dense.h>

#include <doctest.h>

#include <app/Counter.h>

#include <cstddef> // for size_t
#include <utility> // for pair

// very minimal input iterator
// https://en.cppreference.com/w/cpp/named_req/InputIterator#Concept
class It {
    std::pair<Counter::Obj, Counter::Obj> m_kv;

public:
    It(size_t val, Counter& counts)
        : m_kv({val, counts}, {val, counts}) {}

    auto operator++() -> It& {
        ++m_kv.first.get();
        ++m_kv.second.get();
        return *this;
    }

    auto operator*() -> std::pair<Counter::Obj, Counter::Obj> const& {
        return m_kv;
    }

    auto operator!=(It const& other) const -> bool {
        return other.m_kv.first.get() != m_kv.first.get() || other.m_kv.second.get() != m_kv.second.get();
    }
};

TEST_CASE("ctors_map") {
    using Map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>;
    // using Map = std::unordered_map<Counter::Obj, Counter::Obj>;
    using Alloc = Map::allocator_type;
    using Hash = Map::hasher;
    using KeyEq = Map::key_equal;

    Counter counts;
    INFO(counts);

    { auto m = Map{}; }
    { auto m = Map{0, Alloc{}}; }
    { auto m = Map{0, Hash{}, Alloc{}}; }
    { auto m = Map{Alloc{}}; }
    REQUIRE(counts.dtor == 0);

    {
        auto begin_it = It{size_t{0}, counts};
        auto end_it = It{size_t{10}, counts};
        auto m = Map{begin_it, end_it};
        REQUIRE(m.size() == 10);
    }
    {
        auto begin_it = It{size_t{0}, counts};
        auto end_it = It{size_t{10}, counts};
        auto m = Map{begin_it, end_it, 0, Alloc{}};
        REQUIRE(m.size() == 10);
    }
    {
        auto begin_it = It{size_t{0}, counts};
        auto end_it = It{size_t{10}, counts};
        auto m = Map{begin_it, end_it, 0, Hash{}, Alloc{}};
        REQUIRE(m.size() == 10);
    }
    {
        auto begin_it = It{size_t{0}, counts};
        auto end_it = It{size_t{10}, counts};
        auto m = Map{begin_it, end_it, 0, Hash{}, KeyEq{}};
        REQUIRE(m.size() == 10);
    }
}
