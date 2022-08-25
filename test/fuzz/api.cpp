#include <ankerl/unordered_dense.h>
#include <app/counter.h>
#include <fuzz/provider.h>

#if defined(FUZZ)
#    define REQUIRE(x) ::fuzz::provider::require(x) // NOLINT(cppcoreguidelines-macro-usage)
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

void api(uint8_t const* data, size_t size) {
    auto p = fuzz::provider(data, size);
    auto counts = counter();

    using map_t = ankerl::unordered_dense::map<counter::obj, counter::obj>;
    auto map = map_t();
    p.repeat_oneof(
        [&] {
            auto key = p.integral<size_t>();
            auto it = map.try_emplace(counter::obj(key, counts), counter::obj(key, counts)).first;
            REQUIRE(it != map.end());
            REQUIRE(it->first.get() == key);
        },
        [&] {
            auto key = p.integral<size_t>();
            map.emplace(std::piecewise_construct, std::forward_as_tuple(key, counts), std::forward_as_tuple(key + 77, counts));
        },
        [&] {
            auto key = p.integral<size_t>();
            map[counter::obj(key, counts)] = counter::obj(key + 123, counts);
        },
        [&] {
            auto key = p.integral<size_t>();
            map.insert(std::pair<counter::obj, counter::obj>(counter::obj(key, counts), counter::obj(key, counts)));
        },
        [&] {
            auto key = p.integral<size_t>();
            map.insert_or_assign(counter::obj(key, counts), counter::obj(key + 1, counts));
        },
        [&] {
            auto key = p.integral<size_t>();
            map.erase(counter::obj(key, counts));
        },
        [&] {
            map = map_t{};
        },
        [&] {
            auto m = map_t{};
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
            auto it = map.find(counter::obj(key, counts));
            auto d = std::distance(map.begin(), it);
            REQUIRE(0 <= d);
            REQUIRE(d <= static_cast<std::ptrdiff_t>(map.size()));
        },
        [&] {
            if (!map.empty()) {
                auto idx = p.bounded(static_cast<int>(map.size()));
                auto it = map.cbegin() + idx;
                auto const& key = it->first;
                auto found_it = map.find(key);
                REQUIRE(it == found_it);
            }
        },
        [&] {
            if (!map.empty()) {
                auto it = map.begin() + p.bounded(static_cast<int>(map.size()));
                map.erase(it);
            }
        },
        [&] {
            auto tmp = map_t();
            std::swap(tmp, map);
        },
        [&] {
            map = std::initializer_list<std::pair<counter::obj, counter::obj>>{
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
            map.erase(map.cbegin() + first_idx, map.cbegin() + last_idx);
        },
        [&] {
            map.~map_t();
            counts.check_all_done();
            new (&map) map_t();
        },
        [&] {
            std::erase_if(map, [&](map_t::value_type const& /*v*/) {
                return p.integral<bool>();
            });
        });
}

} // namespace fuzz

#if defined(FUZZ)
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(uint8_t const* data, size_t size) -> int {
    fuzz::api(data, size);
    return 0;
}
#endif
