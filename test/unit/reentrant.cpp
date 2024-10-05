#include <ankerl/unordered_dense.h>

#include <app/counter.h>
#include <app/doctest.h>

namespace {

class EraseOnDestruct;

using map_t = ankerl::unordered_dense::map<counter::obj, std::unique_ptr<EraseOnDestruct>>;

class EraseOnDestruct {
    map_t* m_map;
    counter::obj m_counter;

public:
    EraseOnDestruct(map_t& map, size_t i, counter& counts)
        : m_map(&map)
        , m_counter(i, counts) {}

    ~EraseOnDestruct() {
        (void)m_map;
        m_map->erase(counter::obj{m_counter.get() + 1, m_counter.counts()});
    }
};

} // namespace

TEST_CASE("reentrant_destruct") {
    auto counts = counter{};
    INFO(counts);

    auto map = map_t();

    /*
    This doesn't work, it is not something that can be supported with using std::vector.
    I'll leave the code here so it's not lost though.

    for (size_t i = 0; i < 100; ++i) {
        map.try_emplace(counter::obj{i, counts}, std::make_unique<EraseOnDestruct>(map, i, counts));
    }
    map.erase(counter::obj{0, counts});
    */
}
