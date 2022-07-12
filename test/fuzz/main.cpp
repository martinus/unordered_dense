#include <ankerl/unordered_dense.h>
#include <app/Counter.h>
#include <app/robin_hood.h>

#include "Fuzz.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

extern "C" auto LLVMFuzzerTestOneInput(uint8_t const* data, size_t size) -> int {
    auto fuzz = Fuzz(data, size);
    Counter counts;

    using Map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>;
    // using Map = robin_hood::unordered_flat_map<Counter::Obj, Counter::Obj>;
    auto map = Map();
    fuzz.loop_call_any(
        [&] {
            auto key = fuzz.integral<size_t>();
            map.try_emplace(Counter::Obj(key, counts), Counter::Obj(key, counts));
        },
        [&] {
            auto key = fuzz.integral<size_t>();
            map.emplace(std::piecewise_construct, std::forward_as_tuple(key, counts), std::forward_as_tuple(key + 77, counts));
        },
        [&] {
            auto key = fuzz.integral<size_t>();
            map[Counter::Obj(key, counts)] = Counter::Obj(key + 123, counts);
        },
        [&] {
            auto key = fuzz.integral<size_t>();
            map.insert(std::pair<Counter::Obj, Counter::Obj>(Counter::Obj(key, counts), Counter::Obj(key, counts)));
        },
        [&] {
            auto key = fuzz.integral<size_t>();
            map.insert_or_assign(Counter::Obj(key, counts), Counter::Obj(key + 1, counts));
        },
        [&] {
            auto key = fuzz.integral<size_t>();
            map.erase(Counter::Obj(key, counts));
        },
        [&] {
            map = Map{};
        },
        [&] {
            auto m = Map{};
            m.swap(map);
        },
        [&] {
            map.clear();
        },
        [&] {
            auto s = fuzz.range<size_t>(0, 1024);
            map.rehash(s);
        },
        [&] {
            auto s = fuzz.range<size_t>(0, 1024);
            map.reserve(s);
        },
        [&] {
            auto key = fuzz.integral<size_t>();
            auto it = map.find(Counter::Obj(key, counts));
            auto d = std::distance(map.begin(), it);
            Fuzz::require(0 <= d && d <= static_cast<std::ptrdiff_t>(map.size()));
        },
        [&] {
            if (!map.empty()) {
                auto idx = fuzz.range(0, static_cast<int>(map.size() - 1));
                auto it = map.cbegin() + idx;
                auto const& key = it->first;
                auto found_it = map.find(key);
                Fuzz::require(it == found_it);
            }
        },
        [&] {
            if (!map.empty()) {
                auto it = map.begin() + fuzz.range(0, static_cast<int>(map.size() - 1));
                map.erase(it);
            }
        },
        [&] {
            auto tmp = Map();
            std::swap(tmp, map);
        },
        [&] {
            map = std::initializer_list<std::pair<Counter::Obj, Counter::Obj>>{
                {{1, counts}, {2, counts}},
                {{3, counts}, {4, counts}},
                {{5, counts}, {6, counts}},
            };
            Fuzz::require(map.size() == 3);
        },
        [&] {
            auto first_idx = 0;
            auto last_idx = 0;
            if (!map.empty()) {
                first_idx = fuzz.range<int>(0, static_cast<int>(map.size() - 1));
                last_idx = fuzz.range<int>(0, static_cast<int>(map.size() - 1));
                if (first_idx > last_idx) {
                    std::swap(first_idx, last_idx);
                }
            }
            map.erase(map.cbegin() + first_idx, map.cbegin() + last_idx);
        },
        [&] {
            map.~Map();
            counts.check_all_done();
            new (&map) Map();
        });
    return 0;
}
