// Paired A/B of the working-tree header against another revision of it, and optionally against
// boost::unordered_flat_map, using nanobench's compare(): the alternatives run interleaved in the
// same slice of time, so machine drift cancels out and the reported interval is about the ratio.
//
// The two headers coexist in one binary because run.sh renames the baseline's namespace and macro
// prefix (ankerl -> udmbase) into base.h. The workloads are those of bench_quick_overall_udm, from
// the header the benchmark itself uses, plus its all-hits and no-hits lookups.
#include "base.h"

#include <ankerl/unordered_dense.h>
#include <bench/workloads.h>
#include <third-party/nanobench.h>
#ifdef UDM_AB_HAVE_BOOST
#    include <boost/unordered/unordered_flat_map.hpp>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

using base_u64 = udmbase::unordered_dense::map<uint64_t, size_t>;
using base_str = udmbase::unordered_dense::map<std::string, size_t>;
using base_big = udmbase::unordered_dense::map<uint64_t, workloads::big_value>;
#ifdef UDM_AB_GROUP
// The candidate with the group index, against a baseline that is whatever its revision is.
template <typename K, typename V>
using cand_map = ankerl::unordered_dense::map<K,
                                              V,
                                              ankerl::unordered_dense::hash<K>,
                                              std::equal_to<K>,
                                              std::allocator<std::pair<K, V>>,
                                              ankerl::unordered_dense::bucket_type::group>;
#else
template <typename K, typename V>
using cand_map = ankerl::unordered_dense::map<K, V>;
#endif
using cand_u64 = cand_map<uint64_t, size_t>;
using cand_str = cand_map<std::string, size_t>;
using cand_big = cand_map<uint64_t, workloads::big_value>;
#ifdef UDM_AB_HAVE_BOOST
using boost_u64 = boost::unordered_flat_map<uint64_t, size_t, ankerl::unordered_dense::hash<uint64_t>>;
using boost_str = boost::unordered_flat_map<std::string, size_t, ankerl::unordered_dense::hash<std::string>>;
using boost_big = boost::unordered_flat_map<uint64_t, workloads::big_value, ankerl::unordered_dense::hash<uint64_t>>;
#else
using boost_u64 = cand_u64;
using boost_str = cand_str;
using boost_big = cand_big;
#endif

// Workload is a template with one type parameter, the map. The result is whatever it returns;
// doNotOptimizeAway needs it to be a scalar, which is why insert_erase's pair is summed.
template <typename T>
auto scalar(T v) -> T {
    return v;
}

auto scalar(workloads::insert_erase_result r) -> size_t {
    return r.erased + r.size;
}

template <template <typename> class Workload, typename Base, typename Cand, typename Boost>
void compare(char const* name, size_t epochs, bool with_boost) {
    auto bench = ankerl::nanobench::Bench().title(name).epochs(epochs).performanceCounters(false);
    auto base = [] {
        ankerl::nanobench::doNotOptimizeAway(scalar(Workload<Base>::run()));
    };
    auto cand = [] {
        ankerl::nanobench::doNotOptimizeAway(scalar(Workload<Cand>::run()));
    };
    auto boost = [] {
        ankerl::nanobench::doNotOptimizeAway(scalar(Workload<Boost>::run()));
    };
    if (with_boost) {
        bench.compare("base", base, "cand", cand, "boost", boost);
    } else {
        bench.compare("base", base, "cand", cand);
    }
}

// the workloads as templates with one parameter, so that compare() can name them
template <typename Map>
struct iterate {
    static auto run() {
        return workloads::iterate<Map>();
    }
};
template <typename Map>
struct insert_erase {
    static auto run() {
        return workloads::insert_erase<Map>();
    }
};
template <typename Map>
struct build {
    static auto run() {
        return workloads::build<Map>();
    }
};
template <typename Map>
struct churn {
    static auto run() {
        return workloads::churn<Map>();
    }
};
template <typename Map>
struct find_50 {
    static auto run() {
        return workloads::find_50<Map>();
    }
};
template <typename Map>
struct hash_strings {
    static auto run() {
        return workloads::hash_strings<Map>();
    }
};
template <typename Map>
struct find_hits {
    static auto run() {
        return workloads::find_all<Map, true>();
    }
};
template <typename Map>
struct find_misses {
    static auto run() {
        return workloads::find_all<Map, false>();
    }
};

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 2) {
        std::printf("usage: %s <workload|all> [epochs=12] [boost=0]\n"
                    "workloads: it64 ie64 build64 churn64 find64 itstr iestr buildstr churnstr findstr\n"
                    "           itbig iebig buildbig churnbig findbig (64 byte mapped value)\n"
                    "           (bench_quick_overall_udm)\n"
                    "           rhit64 rmiss64 rhitstr rmissstr (all hits / no hits)\n"
                    "           hashstr (the string hash on its own)\n",
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
#define U64(name, workload) \
    if (want(name))         \
    compare<workload, base_u64, cand_u64, boost_u64>(name, epochs, boost)
#define STR(name, workload) \
    if (want(name))         \
    compare<workload, base_str, cand_str, boost_str>(name, epochs, boost)
#define BIG(name, workload) \
    if (want(name))         \
    compare<workload, base_big, cand_big, boost_big>(name, epochs, boost)
    U64("it64", iterate);
    U64("ie64", insert_erase);
    U64("build64", build);
    U64("churn64", churn);
    U64("find64", find_50);
    STR("itstr", iterate);
    STR("iestr", insert_erase);
    STR("buildstr", build);
    STR("churnstr", churn);
    STR("findstr", find_50);
    BIG("itbig", iterate);
    BIG("iebig", insert_erase);
    BIG("buildbig", build);
    BIG("churnbig", churn);
    BIG("findbig", find_50);
    U64("rhit64", find_hits);
    U64("rmiss64", find_misses);
    STR("rhitstr", find_hits);
    STR("rmissstr", find_misses);
    STR("hashstr", hash_strings);
    return 0;
}
