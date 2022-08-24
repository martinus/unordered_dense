#include <ankerl/unordered_dense.h>
#include <app/Counter.h>
#include <fuzz/Provider.h>

#if defined(FUZZ)
#    define REQUIRE(x) ::fuzz::Provider::require(x)
#else
#    include <doctest.h>
#endif

#include <unordered_map>

namespace fuzz {

void replace(uint8_t const* data, size_t size) {
    auto p = fuzz::Provider{data, size};

    auto counts = Counter{};

    using Map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>;

    auto initial_size = p.bounded<size_t>(100);
    auto map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>{};
    for (size_t i = 0; i < initial_size; ++i) {
        map.try_emplace(Counter::Obj{i, counts}, Counter::Obj{i, counts});
    }

    // create a container with data in it provided by fuzzer
    auto container = Map::value_container_type{};
    auto comparison_container = std::vector<std::pair<size_t, size_t>>();
    auto v = size_t{};
    while (p.has_remaining_bytes()) {
        auto key = p.integral<size_t>();
        container.emplace_back(Counter::Obj{key, counts}, Counter::Obj{v, counts});
        comparison_container.emplace_back(key, v);
        ++v;
    }

    // create comparison map with the same move-back-forward algorithm
    auto comparison_map = std::unordered_map<size_t, size_t>{};
    size_t idx = 0;
    while (idx != comparison_container.size()) {
        auto [key, val] = comparison_container[idx];
        if (comparison_map.try_emplace(key, val).second) {
            ++idx;
        } else {
            comparison_container[idx] = comparison_container.back();
            comparison_container.pop_back();
        }
    }

    map.replace(std::move(container));

    // now check if the data in the map is exactly what we expect
    REQUIRE(map.size() == comparison_map.size());
    for (auto [key, val] : comparison_map) {
        auto key_obj = Counter::Obj{key, counts};
        auto val_obj = Counter::Obj{val, counts};
        auto it = map.find(key_obj);
        REQUIRE(it != map.end());
        REQUIRE(it->first == key_obj);
        REQUIRE(it->second == val_obj);
    }
}

} // namespace fuzz

#if defined(FUZZ)
extern "C" auto LLVMFuzzerTestOneInput(uint8_t const* data, size_t size) -> int {
    fuzz::replace(data, size);
    return 0;
}
#endif