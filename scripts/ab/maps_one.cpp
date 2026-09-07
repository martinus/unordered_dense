// One map per binary, one workload per binary, so `perf stat` attributes cleanly and nothing shares
// a translation unit with a rival.
//
// A paired two-header measurement decides a 10% question and not a 3% one -- the code layout of a
// binary holding several maps moves by more than that every time any of them changes. Anything
// smaller than 10% is decided here instead, with counters.
//
//   -DUDM_ONE_MAP=<index into maps_for<Key>::type>   -DUDM_ONE_STR   to use string keys
//   argv: <hit|miss|half|build|churn|ie|iterate|none> [entries] [reps]
//
// `none` runs the workload's loop with no map in it at all, which is what the other numbers are
// netted against.

#include "maps.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifndef UDM_ONE_MAP
#    define UDM_ONE_MAP 0
#endif

namespace {
using namespace udm_maps;

#if defined(UDM_ONE_STR)
using one_key = std::string;
using one_val = std::size_t;
#elif defined(UDM_ONE_BIG)
using one_key = std::uint64_t;
using one_val = workloads::big_value;
#else
using one_key = std::uint64_t;
using one_val = std::size_t;
#endif
using map_t = std::tuple_element_t<UDM_ONE_MAP, typename maps_for<one_key, one_val>::type>;

} // namespace

int main(int argc, char** argv) {
    workloads::tame_allocator();
    auto const what = std::string(argc > 1 ? argv[1] : "hit");
    auto const n = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 50000;
    auto const reps = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 30000000;

    auto p = pools<one_key>(n);
    auto acc = std::size_t{0};

    if (what == "build") {
        for (std::size_t i = 0; i < reps; ++i) {
            acc += build<map_t>(p);
        }
    } else if (what == "none") {
        // the lookup loop's own cost: the rng, the index arithmetic and the key selection
        auto st = lookup_state();
        for (std::size_t i = 0; i < reps; ++i) {
            auto const at = static_cast<std::size_t>(((st.rng() >> 32U) * p.present.size()) >> 32U);
            ankerl::nanobench::doNotOptimizeAway(&p.present[at]);
            acc += at;
        }
    } else {
        auto m = map_t();
        fill(m, p);
        if (what == "churn" || what == "ie") {
            // measure a table that has been churning, not a freshly built one
            auto rng = ankerl::nanobench::Rng(7);
            auto tick = std::size_t{0};
            if (what == "churn") {
                churn(m, p.present, p.spare, rng, reps, tick);
                acc += m.size();
            } else {
                acc += insert_erase(m, p, rng, reps, tick);
            }
        } else if (what == "iterate") {
            for (std::size_t i = 0; i < reps; ++i) {
                acc += m.sum();
            }
        } else {
            auto const ask = what == "hit" ? asking::hits : (what == "miss" ? asking::misses : asking::half);
            auto st = lookup_state();
            acc += lookups(m, p, st, ask, reps);
        }
    }
    std::printf("%s %s n=%zu reps=%zu acc=%zu\n", map_t::name, what.c_str(), n, reps, acc);
}
