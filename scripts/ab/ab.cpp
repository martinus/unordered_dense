// Paired A/B of the working-tree header against another revision of it, and optionally against
// boost::unordered_flat_map, using nanobench's compare(): the alternatives run interleaved in the
// same slice of time, so machine drift cancels out and the reported interval is about the ratio.
//
// The two headers coexist in one binary because run.sh renames the baseline's namespace and macro
// prefix (ankerl -> udmbase) into base.h. The workloads replicate bench_quick_overall_udm exactly,
// plus all-hits and no-hits lookups, which bound what a workload's hit rate can do to the probe.
#include "base.h"

#include <ankerl/unordered_dense.h>
#include <nanobench.h>
#ifdef UDM_AB_HAVE_BOOST
#    include <boost/unordered/unordered_flat_map.hpp>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

template <typename K>
auto init_key() -> K {
    return {};
}

template <>
auto init_key<std::string>() -> std::string {
    return std::string(200, '\0');
}

template <typename T>
void randomize_key(ankerl::nanobench::Rng* rng, int n, T* key) {
    auto limited = (((*rng)() >> 32U) * static_cast<uint64_t>(n)) >> 32U;
    *key = static_cast<T>(limited);
}

void randomize_key(ankerl::nanobench::Rng* rng, int n, std::string* key) {
    uint64_t k{};
    randomize_key(rng, n, &k);
    std::memcpy(key->data(), &k, sizeof(k));
}

template <typename K>
void set_key(uint64_t v, K* key) {
    *key = static_cast<K>(v);
}

void set_key(uint64_t v, std::string* key) {
    std::memcpy(key->data(), &v, sizeof(v));
}

// the three quick_overall workloads, verbatim
template <typename Map>
auto insert_erase() -> uint64_t {
    ankerl::nanobench::Rng rng(123);
    size_t verifier{};
    Map map;
    auto key = init_key<typename Map::key_type>();
    for (int n = 1; n < 20000; ++n) {
        for (int i = 0; i < 200; ++i) {
            randomize_key(&rng, n, &key);
            map[key];
            randomize_key(&rng, n, &key);
            verifier += map.erase(key);
        }
    }
    return verifier + map.size();
}

template <typename Map>
auto find_50() -> uint64_t {
    ankerl::nanobench::Rng insert_rng(123123);
    ankerl::nanobench::Rng search_rng(987654321);
    constexpr auto never_inserted = uint64_t{1} << 63U;
    std::vector<uint64_t> inserted;
    size_t checksum = 0;
    Map map;
    auto key = init_key<typename Map::key_type>();
    for (size_t i = 0; i < 100000; ++i) {
        auto candidate = insert_rng() & ~never_inserted;
        if (inserted.empty() || (insert_rng() & 1U) != 0) {
            set_key(candidate, &key);
            if (map.emplace(key, i).second) {
                inserted.push_back(candidate);
            }
        }
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

template <typename Map>
auto iterate() -> uint64_t {
    auto key = init_key<typename Map::key_type>();
    ankerl::nanobench::Rng rng(555);
    Map map;
    size_t result = 0;
    for (size_t n = 0; n < 5000; ++n) {
        randomize_key(&rng, 1000000, &key);
        map[key] = n;
        for (auto const& kv : map) {
            result += kv.second;
        }
    }
    rng = ankerl::nanobench::Rng(555);
    do {
        randomize_key(&rng, 1000000, &key);
        map.erase(key);
        for (auto const& kv : map) {
            result += kv.second;
        }
    } while (!map.empty());
    return result;
}

// 50k entries, then 10M lookups whose outcome is decided by an independent rng: HitPct of them
// find a key that is there, the rest look for one that cannot be.
template <typename Map, int HitPct>
auto random_find() -> uint64_t {
    ankerl::nanobench::Rng rng(999);
    Map map;
    std::vector<uint64_t> keys;
    auto key = init_key<typename Map::key_type>();
    while (map.size() < 50000) {
        auto v = rng() & ((uint64_t{1} << 30U) - 1);
        set_key(v, &key);
        if (map.emplace(key, keys.size()).second) {
            keys.push_back(v);
        }
    }
    size_t checksum = 0;
    for (size_t i = 0; i < 10000000; ++i) {
        auto r = rng();
        bool hit = HitPct == 100 || (HitPct != 0 && (r & 1U));
        set_key(hit ? keys[(r >> 1U) % keys.size()] : ((uint64_t{1} << 30U) | (r >> 1U)), &key);
        auto it = map.find(key);
        if (it != map.end()) {
            checksum += it->second;
        }
    }
    return checksum;
}

template <typename Base, typename Cand, typename Boost, typename Fn>
void compare(char const* name, size_t epochs, bool with_boost, Fn fn) {
    auto bench = ankerl::nanobench::Bench().title(name).epochs(epochs).performanceCounters(false);
    auto base = [&] {
        ankerl::nanobench::doNotOptimizeAway(fn(static_cast<Base*>(nullptr)));
    };
    auto cand = [&] {
        ankerl::nanobench::doNotOptimizeAway(fn(static_cast<Cand*>(nullptr)));
    };
    auto boost = [&] {
        ankerl::nanobench::doNotOptimizeAway(fn(static_cast<Boost*>(nullptr)));
    };
    if (with_boost) {
        bench.compare("base", base, "cand", cand, "boost", boost);
    } else {
        bench.compare("base", base, "cand", cand);
    }
}

#define UDM_AB_WORKLOAD(fn)                              \
    [](auto* m) {                                        \
        return fn<std::remove_pointer_t<decltype(m)>>(); \
    }

using base_u64 = udmbase::unordered_dense::map<uint64_t, size_t>;
using cand_u64 = ankerl::unordered_dense::map<uint64_t, size_t>;
using base_str = udmbase::unordered_dense::map<std::string, size_t>;
using cand_str = ankerl::unordered_dense::map<std::string, size_t>;
#ifdef UDM_AB_HAVE_BOOST
using boost_u64 = boost::unordered_flat_map<uint64_t, size_t, ankerl::unordered_dense::hash<uint64_t>>;
using boost_str = boost::unordered_flat_map<std::string, size_t, ankerl::unordered_dense::hash<std::string>>;
#else
using boost_u64 = cand_u64;
using boost_str = cand_str;
#endif

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 2) {
        std::printf("usage: %s <workload|all> [epochs=12] [boost=0]\n"
                    "workloads: it64 ie64 find64 itstr iestr findstr (bench_quick_overall_udm)\n"
                    "           rhit64 rmiss64 rhitstr rmissstr (all hits / no hits)\n",
                    argv[0]);
        return 1;
    }
    std::string w = argv[1];
    auto epochs = static_cast<size_t>(argc > 2 ? std::atoi(argv[2]) : 12);
    bool boost = argc > 3 && std::atoi(argv[3]) != 0;
#ifndef UDM_AB_HAVE_BOOST
    if (boost) {
        std::printf("built without boost, see run.sh\n");
        return 1;
    }
#endif
    auto want = [&](char const* n) {
        return w == "all" || w == n;
    };
#define U64(name, fn) \
    if (want(name))   \
    compare<base_u64, cand_u64, boost_u64>(name, epochs, boost, UDM_AB_WORKLOAD(fn))
#define STR(name, fn) \
    if (want(name))   \
    compare<base_str, cand_str, boost_str>(name, epochs, boost, UDM_AB_WORKLOAD(fn))
    U64("it64", iterate);
    U64("ie64", insert_erase);
    U64("find64", find_50);
    STR("itstr", iterate);
    STR("iestr", insert_erase);
    STR("findstr", find_50);
    if (want("rhit64"))
        compare<base_u64, cand_u64, boost_u64>("rhit64", epochs, boost, [](auto* m) {
            return random_find<std::remove_pointer_t<decltype(m)>, 100>();
        });
    if (want("rmiss64"))
        compare<base_u64, cand_u64, boost_u64>("rmiss64", epochs, boost, [](auto* m) {
            return random_find<std::remove_pointer_t<decltype(m)>, 0>();
        });
    if (want("rhitstr"))
        compare<base_str, cand_str, boost_str>("rhitstr", epochs, boost, [](auto* m) {
            return random_find<std::remove_pointer_t<decltype(m)>, 100>();
        });
    if (want("rmissstr"))
        compare<base_str, cand_str, boost_str>("rmissstr", epochs, boost, [](auto* m) {
            return random_find<std::remove_pointer_t<decltype(m)>, 0>();
        });
    return 0;
}
