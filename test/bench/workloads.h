#pragma once

// The workloads of bench_quick_overall_udm, as functions of the map type that return what the
// benchmark checks. Shared with scripts/ab, whose paired A/B replicates the benchmark exactly --
// exactly because there is one copy.

#include <third-party/nanobench.h> // for Rng

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace workloads {

template <typename K>
inline auto init_key() -> K {
    return {};
}

template <>
inline auto init_key<std::string>() -> std::string {
    std::string str;
    str.resize(200);
    return str;
}

// the first 8 bytes of a string key carry the value; the rest stays zero
template <typename T>
inline void set_key(uint64_t v, T* key) {
    *key = static_cast<T>(v);
}

inline void set_key(uint64_t v, std::string* key) {
    std::memcpy(key->data(), &v, sizeof(v));
}

// a random key in [0, n)
template <typename T>
inline void randomize_key(ankerl::nanobench::Rng* rng, int n, T* key) {
    // we limit ourselves to 32bit n
    auto limited = (((*rng)() >> 32U) * static_cast<uint64_t>(n)) >> 32U;
    set_key(limited, key);
}

struct insert_erase_result {
    size_t erased;
    size_t size;
};

// Random insert & erase, ~10k entries live
template <typename Map>
auto insert_erase() -> insert_erase_result {
    ankerl::nanobench::Rng rng(123);
    size_t erased{};
    Map map;
    auto key = init_key<typename Map::key_type>();
    for (int n = 1; n < 20000; ++n) {
        for (int i = 0; i < 200; ++i) {
            randomize_key(&rng, n, &key);
            map[key];
            randomize_key(&rng, n, &key);
            erased += map.erase(key);
        }
    }
    return {erased, map.size()};
}

// iterate while adding, then while removing
template <typename Map>
auto iterate() -> size_t {
    size_t const num_elements = 5000;
    auto key = init_key<typename Map::key_type>();
    ankerl::nanobench::Rng rng(555);
    Map map;
    size_t result = 0;
    for (size_t n = 0; n < num_elements; ++n) {
        randomize_key(&rng, 1000000, &key);
        map[key] = n;
        for (auto const& key_val : map) {
            result += key_val.second;
        }
    }

    rng = ankerl::nanobench::Rng(555);
    do {
        randomize_key(&rng, 1000000, &key);
        map.erase(key);
        for (auto const& key_val : map) {
            result += key_val.second;
        }
    } while (!map.empty());
    return result;
}

// Random finds against a map that grows to ~50k entries while it is searched. Half of the
// lookups find a key that is in the map, chosen at random among those; the other half look for a
// key that cannot be. Which of the two each lookup is, is decided by an rng of its own, so nothing
// here repeats. It used to: the searches replayed the insertion sequence from the same seed, and
// a branch predictor with a long history learned half of the outcomes (0.6 mispredictions per
// lookup where a random sequence costs 1.35). A change that trades instructions for
// mispredictions looked worse here than it was.
template <typename Map>
auto find_50() -> size_t {
    ankerl::nanobench::Rng insert_rng(123123);
    ankerl::nanobench::Rng search_rng(987654321);
    constexpr auto never_inserted = uint64_t{1} << 63U;

    std::vector<uint64_t> inserted;
    inserted.reserve(100000);
    size_t checksum = 0;
    Map map;
    auto key = init_key<typename Map::key_type>();
    for (size_t i = 0; i < 100000; ++i) {
        // half of the candidates go in; the first one always, so there is something to find
        auto candidate = insert_rng() & ~never_inserted;
        if (inserted.empty() || (insert_rng() & 1U) != 0) {
            set_key(candidate, &key);
            if (map.emplace(key, i).second) {
                inserted.push_back(candidate);
            }
        }

        // search 100 entries in the map
        for (size_t search = 0; search < 100; ++search) {
            auto r = search_rng();
            if ((r & 1U) != 0) {
                set_key(inserted[((r >> 32U) * inserted.size()) >> 32U], &key);
            } else {
                set_key(r | never_inserted, &key);
            }
            auto it = map.find(key);
            if (it != map.end()) {
                checksum += it->second;
            }
        }
    }
    return checksum;
}

// 50k entries, then 10M lookups that all hit (a random key that is there) or all miss (one that
// cannot be): the two ends of what a workload's hit rate can do to the probe.
template <typename Map, bool Hits>
auto find_all() -> size_t {
    ankerl::nanobench::Rng rng(999);
    constexpr auto never_inserted = uint64_t{1} << 63U;
    Map map;
    std::vector<uint64_t> keys;
    keys.reserve(50000);
    auto key = init_key<typename Map::key_type>();
    while (map.size() < 50000) {
        auto v = rng() & ~never_inserted;
        set_key(v, &key);
        if (map.emplace(key, keys.size()).second) {
            keys.push_back(v);
        }
    }
    size_t checksum = 0;
    auto const num_keys = keys.size();
    for (size_t i = 0; i < 10000000; ++i) {
        auto r = rng();
        set_key(Hits ? keys[((r >> 32U) * num_keys) >> 32U] : (r | never_inserted), &key);
        auto it = map.find(key);
        if (it != map.end()) {
            checksum += it->second;
        }
    }
    return checksum;
}

} // namespace workloads
