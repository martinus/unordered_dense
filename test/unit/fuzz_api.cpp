#include <ankerl/unordered_dense.h>
#include <app/doctest.h>
#include <fuzz/provider.h>
#include <fuzz/run.h>

#include <stdexcept>
#include <unordered_map>
#include <vector>

// Mirrors every operation into a std::unordered_map and compares. Without a reference the target
// only finds the bugs that crash; with one it finds a map that stays alive while answering wrong,
// which is the more likely kind. The reference holds plain size_t, so it does not disturb the
// counter that checks every counter::obj is destroyed exactly once.
template <typename Map>
void do_fuzz_api(fuzz::provider p) {
    auto counts = counter();
    auto map = Map();
    auto ref = std::unordered_map<size_t, size_t>();

    // Cheap after every operation. The full comparison is the expensive one and runs at the end.
    auto check_size = [&] {
        REQUIRE(map.size() == ref.size());
        REQUIRE(map.empty() == ref.empty());
    };

    // Whole-container checks cost a lookup per element, so on a big map they cost more executions
    // than they are worth -- and a bug that only a big map can show is one no fuzzer would shrink
    // to a readable reproducer anyway. Above this many elements the same operations still run,
    // they are just checked by size rather than element by element.
    static constexpr auto deep_check_limit = size_t{256};

    // For the operations whose effect is easier to observe than to predict -- a predicate the
    // fuzzer chooses, a key picked by position -- take the map's word for it rather than give up
    // on having a reference at all.
    auto resync = [&] {
        ref.clear();
        for (auto const& [key, value] : map) {
            ref.emplace(key.get(), value.get());
        }
    };

    p.repeat_oneof(
        [&] {
            auto key = p.integral<size_t>();
            auto it = map.try_emplace(counter::obj(key, counts), counter::obj(key, counts)).first;
            REQUIRE(it != map.end());
            REQUIRE(it->first.get() == key);
            ref.try_emplace(key, key);
            check_size();
        },
        [&] {
            auto key = p.integral<size_t>();
            map.emplace(std::piecewise_construct, std::forward_as_tuple(key, counts), std::forward_as_tuple(key + 77, counts));
            ref.emplace(key, key + 77);
            check_size();
        },
        [&] {
            auto key = p.integral<size_t>();
            map[counter::obj(key, counts)] = counter::obj(key + 123, counts);
            ref[key] = key + 123;
            check_size();
        },
        [&] {
            auto key = p.integral<size_t>();
            map.insert(std::pair<counter::obj, counter::obj>(counter::obj(key, counts), counter::obj(key, counts)));
            ref.insert(std::pair<size_t, size_t>(key, key));
            check_size();
        },
        [&] {
            auto key = p.integral<size_t>();
            map.insert_or_assign(counter::obj(key, counts), counter::obj(key + 1, counts));
            ref.insert_or_assign(key, key + 1);
            check_size();
        },
        [&] {
            auto key = p.integral<size_t>();
            REQUIRE(map.erase(counter::obj(key, counts)) == ref.erase(key));
            check_size();
        },
        [&] {
            map = Map{};
            ref.clear();
            check_size();
        },
        [&] {
            auto m = Map{};
            m.swap(map);
            ref.clear();
            check_size();
        },
        [&] {
            map.clear();
            ref.clear();
            check_size();
        },
        [&] {
            auto s = p.bounded<size_t>(1025);
            map.rehash(s);
            check_size();
        },
        [&] {
            auto s = p.bounded<size_t>(1025);
            map.reserve(s);
            check_size();
        },
        [&] {
            auto key = p.integral<size_t>();
            auto it = map.find(counter::obj(key, counts));
            auto d = std::distance(map.begin(), it);
            REQUIRE(0 <= d);
            REQUIRE(d <= static_cast<std::ptrdiff_t>(map.size()));
            // the reference decides whether it should have been found
            REQUIRE((it != map.end()) == (ref.find(key) != ref.end()));
            REQUIRE(map.contains(counter::obj(key, counts)) == (ref.count(key) == 1));
            REQUIRE(map.count(counter::obj(key, counts)) == ref.count(key));
        },
        [&] {
            // at() on a key that is there, and on one that is not: the missing case is the only
            // way into on_error_key_not_found()
            auto key = p.integral<size_t>();
            auto found = ref.find(key);
            if (found == ref.end()) {
                REQUIRE_THROWS_AS(map.at(counter::obj(key, counts)), std::out_of_range);
            } else {
                REQUIRE(map.at(counter::obj(key, counts)).get() == found->second);
            }
        },
        [&] {
            auto key = p.integral<size_t>();
            auto range = map.equal_range(counter::obj(key, counts));
            REQUIRE(std::distance(range.first, range.second) == static_cast<std::ptrdiff_t>(ref.count(key)));
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
                auto idx = p.bounded(static_cast<int>(map.size()));
                auto it = map.begin() + idx;
                auto key = it->first.get();
                map.erase(it);
                REQUIRE(ref.erase(key) == 1);
                check_size();
            }
        },
        [&] {
            auto tmp = Map();
            std::swap(tmp, map);
            ref.clear();
            check_size();
        },
        [&] {
            // Copying a map that has something in it, which nothing else here does: it is the only
            // way into the half of copy_buckets() that copies buckets rather than allocating empty
            // ones, and the copy has to answer everything the original does.
            auto copy = map;
            REQUIRE(copy.size() == map.size());
            if (map.size() <= deep_check_limit) {
                for (auto const& [key, value] : map) {
                    auto it = copy.find(key);
                    REQUIRE(it != copy.end());
                    REQUIRE(it->second.get() == value.get());
                }
                REQUIRE(copy == map);
            }
            // and it has to survive being modified, having been built from someone else's buckets
            auto key = p.integral<size_t>();
            copy[counter::obj(key, counts)] = counter::obj(key, counts);
            REQUIRE(copy.contains(counter::obj(key, counts)));
        },
        [&] {
            // copy assignment onto a map that already holds something
            auto other = Map();
            auto key = p.integral<size_t>();
            other.try_emplace(counter::obj(key, counts), counter::obj(key, counts));
            other = map;
            REQUIRE(other.size() == map.size());
            if (map.size() <= deep_check_limit) {
                REQUIRE(other == map);
            }
        },
        [&] {
            // Self assignment and self comparison, both of which have an early out that nothing
            // else here reaches, and both of which are where a careless implementation drops
            // everything. Through a reference, or clang's -Wself-assign-overloaded fires.
            auto& alias = map;
            map = alias;
            REQUIRE(map.size() == ref.size());
            if (map.size() <= deep_check_limit) {
                REQUIRE(map == alias);
            }
            check_size();
        },
        [&] {
            // Two maps that differ, so operator== has to say so: once by size, once by a value
            // under a key both of them have. Only the equal case was ever exercised.
            auto other = map;
            auto key = p.integral<size_t>();
            if (other.erase(counter::obj(key, counts)) == 1) {
                REQUIRE(other.size() + 1 == map.size());
                REQUIRE(!(other == map));
                REQUIRE(other != map);
            }
            if (!map.empty() && map.size() <= deep_check_limit) {
                auto again = map;
                auto it = again.begin() + p.bounded(static_cast<int>(again.size()));
                it->second = counter::obj(it->second.get() + 1, counts);
                REQUIRE(again.size() == map.size());
                REQUIRE(!(again == map));
            }
        },
        [&] {
            // the bucket_count constructor, which reserves up front instead of growing
            auto bucket_count = p.bounded<size_t>(513);
            auto sized = Map(bucket_count);
            REQUIRE(sized.empty());
            auto key = p.integral<size_t>();
            sized.try_emplace(counter::obj(key, counts), counter::obj(key, counts));
            REQUIRE(sized.size() == 1);
        },
        [&] {
            map = std::initializer_list<std::pair<counter::obj, counter::obj>>{
                {{1, counts}, {2, counts}},
                {{3, counts}, {4, counts}},
                {{5, counts}, {6, counts}},
            };
            REQUIRE(map.size() == 3);
            ref = {{1, 2}, {3, 4}, {5, 6}};
            check_size();
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
            auto erased = std::vector<size_t>();
            for (auto it = map.cbegin() + first_idx; it != map.cbegin() + last_idx; ++it) {
                erased.push_back(it->first.get());
            }
            map.erase(map.cbegin() + first_idx, map.cbegin() + last_idx);
            for (auto key : erased) {
                REQUIRE(ref.erase(key) == 1);
            }
            check_size();
        },
        [&] {
            map.~Map();
            counts.check_all_done();
            new (&map) Map();
            ref.clear();
            check_size();
        },
        [&] {
            std::erase_if(map, [&](typename Map::value_type const& /*v*/) {
                return p.integral<bool>();
            });
            resync();
        });

    // Everything the map still holds has to be what the reference holds, and nothing else.
    REQUIRE(map.size() == ref.size());
    for (auto const& [key, value] : map) {
        auto it = ref.find(key.get());
        REQUIRE(it != ref.end());
        REQUIRE(it->second == value.get());
    }
    // and nothing the reference has went missing from the map
    for (auto const& [key, value] : ref) {
        auto it = map.find(counter::obj(key, counts));
        REQUIRE(it != map.end());
        REQUIRE(it->second.get() == value);
    }
}

FUZZ_TEST_CASE(fuzz_api, p) {
    switch (p.bounded<size_t>(3)) {
    case 0:
        do_fuzz_api<ankerl::unordered_dense::map<counter::obj, counter::obj>>(p.copy());
        break;
    case 1:
        do_fuzz_api<ankerl::unordered_dense::segmented_map<counter::obj, counter::obj>>(p.copy());
        break;
    default:
        do_fuzz_api<deque_map<counter::obj, counter::obj>>(p.copy());
        break;
    }
}
