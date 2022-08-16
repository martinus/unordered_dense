#include <ankerl/unordered_dense.h>
#include <app/Counter.h>

#include "Provider.h"

#if defined(FUZZ)
#    define REQUIRE(x) ::fuzz::Provider::require(x)
#else
#    include <doctest.h>
#endif

#include <initializer_list> // for initializer_list
#include <iterator>         // for distance, __iterator_traits<>::d...
#include <new>              // for operator new
#include <tuple>            // for forward_as_tuple
#include <utility>          // for swap, pair, piecewise_construct
#include <vector>           // for vector

namespace fuzz {

template <typename It>
inline auto advance(It it, int num) -> It {
    for (int i = 0; i < num; ++i) {
        ++it;
    }
    return it;
}

void api(uint8_t const* data, size_t size) {
    auto p = fuzz::Provider(data, size);
    Counter counts;

    using Map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>;
    auto map = Map();
    p.repeat_oneof(
        [&] {
            auto key = p.integral<size_t>();
            auto it = map.try_emplace(Counter::Obj(key, counts), Counter::Obj(key, counts)).first;
            REQUIRE(it != map.end());
            REQUIRE(it->first.get() == key);
        },
        [&] {
            auto key = p.integral<size_t>();
            map.emplace(std::piecewise_construct, std::forward_as_tuple(key, counts), std::forward_as_tuple(key + 77, counts));
        },
        [&] {
            auto key = p.integral<size_t>();
            map[Counter::Obj(key, counts)] = Counter::Obj(key + 123, counts);
        },
        [&] {
            auto key = p.integral<size_t>();
            map.insert(std::pair<Counter::Obj, Counter::Obj>(Counter::Obj(key, counts), Counter::Obj(key, counts)));
        },
        [&] {
            auto key = p.integral<size_t>();
            map.insert_or_assign(Counter::Obj(key, counts), Counter::Obj(key + 1, counts));
        },
        [&] {
            auto key = p.integral<size_t>();
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
            auto s = p.bounded<size_t>(1025);
            map.rehash(s);
        },
        [&] {
            auto s = p.bounded<size_t>(1025);
            map.reserve(s);
        },
        [&] {
            auto key = p.integral<size_t>();
            auto it = map.find(Counter::Obj(key, counts));
            auto d = std::distance(map.begin(), it);
            REQUIRE(0 <= d);
            REQUIRE(d <= static_cast<std::ptrdiff_t>(map.size()));
        },
        [&] {
            if (!map.empty()) {
                auto idx = p.bounded(static_cast<int>(map.size()));
                auto it = advance(map.cbegin(), idx);
                auto const& key = it->first;
                auto found_it = map.find(key);
                REQUIRE(it == found_it);
            }
        },
        [&] {
            if (!map.empty()) {
                auto it = advance(map.begin(), p.bounded(static_cast<int>(map.size())));
                map.erase(it);
            }
        },
        [&] {
            auto tmp = Map();
            std::swap(tmp, map);
        },
        [&] {
            map = std::initializer_list<std::pair<const Counter::Obj, Counter::Obj>>{
                {{1, counts}, {2, counts}},
                {{3, counts}, {4, counts}},
                {{5, counts}, {6, counts}},
            };
            REQUIRE(map.size() == 3);
        },
        [&] {
            auto first_idx = 0;
            auto last_idx = 0;
            if (!map.empty()) {
                first_idx = p.bounded(static_cast<int>(map.size()));
                last_idx = p.bounded(static_cast<int>(map.size()));
                if (first_idx > last_idx) {
                    std::swap(first_idx, last_idx);
                }
            }
            map.erase(advance(map.cbegin(), first_idx), advance(map.cbegin(), last_idx));
        },
        [&] {
            map.~Map();
            counts.check_all_done();
            new (&map) Map();
        },
        [&] {
            for (auto i = map.begin(), last = map.end(); i != last;) {
                if (p.integral<bool>()) {
                    i = map.erase(i);
                } else {
                    ++i;
                }
            }
        });
}

} // namespace fuzz

#if defined(FUZZ)
extern "C" auto LLVMFuzzerTestOneInput(uint8_t const* data, size_t size) -> int {
    fuzz::api(data, size);
    return 0;
}
#endif
