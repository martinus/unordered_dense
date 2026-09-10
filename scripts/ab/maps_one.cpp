// One map per binary, one workload per binary, so `perf stat` attributes cleanly and nothing shares
// a translation unit with a rival.
//
// A paired two-header measurement decides a 10% question and not a 3% one -- the code layout of a
// binary holding several maps moves by more than that every time any of them changes. Anything
// smaller than 10% is decided here instead, with counters.
//
//   -DUDM_ONE_MAP=<index into maps_for<Key>::type>   -DUDM_ONE_STR   to use string keys
//   argv: <hit|miss|half|insert|bump|build|churn|ie|iterate|none> [entries] [reps]
//
// `none` runs the workload's loop with no map in it at all, which is what the other numbers are
// netted against.
//
// `insert` and `bump` are the two halves of the insert path, and `reps` counts operations for both,
// so every column comes out per insert and per bump. `insert` builds a *reserved* map, so what it
// measures has no growth and no rehash in it; `bump` is ++m[k] on a key already present. Their
// bodies sit in noinline functions on purpose -- `objdump -d` then has a symbol to disassemble, and
// the question these two modes exist for is which instructions are in that symbol.

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

// One round of each, out of line so that the disassembly has a name to look for. The map itself
// still inlines into these exactly as it does into any caller; the counts are the check that it
// does.
template <typename Key>
[[gnu::noinline]] auto insert_round(pools<Key> const& p) -> std::size_t {
    return build_reserved<map_t>(p);
}

template <typename Key>
[[gnu::noinline]] auto bump_round(map_t& m, pools<Key> const& p, lookup_state& st, std::size_t n) -> std::size_t {
    return bump_present(m, p, st, n);
}

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
    } else if (what == "insert") {
        // reps counts inserts, not rounds, so the per-op columns are per insert. What is left in
        // them besides the insert is one allocate-and-free pair per n of them -- under a tenth of an
        // instruction per insert at n = 50000, and the same for every map -- so it is not netted out.
        auto const rounds = reps < n ? std::size_t{1} : static_cast<std::size_t>(reps / n);
        for (std::size_t i = 0; i < rounds; ++i) {
            acc += insert_round(p);
        }
    } else if (what == "bump") {
        auto m = map_t();
        fill(m, p);
        auto st = lookup_state();
        acc += bump_round(m, p, st, reps);
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
    std::printf("%s %s n=%zu reps=%zu acc=%zu%s\n",
                map_t::name,
                what.c_str(),
                n,
                reps,
                acc,
                what == "insert" && !map_t::reserves ? " (cannot reserve: this is a growing build)" : "");
}
