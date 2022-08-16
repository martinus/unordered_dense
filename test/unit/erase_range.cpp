#include <ankerl/unordered_dense.h>

#include <app/Counter.h>
#include <app/nanobench.h>

#include <doctest.h>

#include <cstddef> // for size_t
#include <vector>  // for vector

namespace {

template <typename It>
inline auto advance(It it, int num) -> It {
    for (int i = 0; i < num; ++i) {
        ++it;
    }
    return it;
}

} // namespace

TEST_CASE("erase_range") {
    int const num_elements = 10;

    for (int first_pos = 0; first_pos <= num_elements; ++first_pos) {
        for (int last_pos = first_pos; last_pos <= num_elements; ++last_pos) {
            Counter counts;
            INFO(counts);

            auto map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>();

            for (size_t i = 0; i < num_elements; ++i) {
                auto key = i;
                auto val = i * 1000;
                map.try_emplace({key, counts}, val, counts);
            }
            REQUIRE(map.size() == num_elements);

            auto it_ret = map.erase(advance(map.cbegin(), first_pos), advance(map.cbegin(), last_pos));
            REQUIRE(map.size() == num_elements - (last_pos - first_pos));
            REQUIRE(it_ret == advance(map.begin(), first_pos));
        }
    }
}
