// Every index structure in the blog post, on the same workloads, in one process.
//
// nanobench's compare() runs the alternatives interleaved round by round, so a clock ramp or a
// noisy neighbour cancels out of the ratio. Every size-sensitive workload is measured at several
// sizes across one octave and summarised by the geometric mean, because these maps have different
// maximum loads (0.8, 0.875, 0.9, 0.5) and so double at different sizes: a ratio at one size is a
// ratio at one arbitrary point of each map's own load-factor sawtooth.
//
//   scripts/ab/maps.sh <check|speed|memory|names> [u64|str|big] [base]

#include "maps.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
using namespace udm_maps;

// ------------------------------------------------------------------ driver

template <typename Tuple>
constexpr std::size_t num_maps = std::tuple_size_v<Tuple>;

template <typename Tuple, std::size_t... I>
auto names_of(std::index_sequence<I...>) -> std::vector<char const*> {
    return {std::tuple_element_t<I, Tuple>::name...};
}

// compare() takes name/op pairs; a tuple_cat of one pair per map turns the type list into them.
template <typename Tuple, typename F, std::size_t... I>
auto compare_all(ankerl::nanobench::Bench& b, F&& f, std::index_sequence<I...> /*seq*/) {
    auto args = std::tuple_cat(
        std::make_tuple(std::tuple_element_t<I, Tuple>::name, f(std::integral_constant<std::size_t, I>{}))...);
    return std::apply([&](auto&&... a) { return b.compare(a...); }, args);
}

struct acc_row {
    std::vector<double> logsum;
    std::size_t n{};
    explicit acc_row(std::size_t k)
        : logsum(k, 0.0) {}
};

std::FILE* g_csv = nullptr;
char const* g_keyname = "u64";

template <typename Tuple>
void print_point(char const* what, std::size_t n, std::vector<double> const& ns, acc_row& acc) {
    static auto const names = names_of<Tuple>(std::make_index_sequence<num_maps<Tuple>>{});
    std::printf("  %-8s n=%-8zu", what, n);
    for (std::size_t a = 0; a < ns.size(); ++a) {
        std::printf(" %s=%.2f", names[a], ns[a]);
        acc.logsum[a] += std::log(ns[a]);
    }
    ++acc.n;
    std::printf("\n");
    std::fflush(stdout);
}

template <typename Tuple>
void print_geomean(char const* what, std::size_t base, acc_row const& acc) {
    if (acc.n == 0) {
        return;
    }
    static auto const names = names_of<Tuple>(std::make_index_sequence<num_maps<Tuple>>{});
    std::printf("# %-8s octave@%-8zu", what, base);
    for (std::size_t a = 0; a < acc.logsum.size(); ++a) {
        auto const g = std::exp(acc.logsum[a] / static_cast<double>(acc.n));
        std::printf(" %s=%.2f", names[a], g);
        if (g_csv != nullptr) {
            std::fprintf(g_csv, "%s,%s,%zu,%s,%.4f,%.4f\n", g_keyname, what, base, names[a], g,
                         g / std::exp(acc.logsum[0] / static_cast<double>(acc.n)));
        }
    }
    std::printf("\n");
    std::fflush(stdout);
    if (g_csv != nullptr) {
        std::fflush(g_csv);
    }
}

double g_interval = 0.03;
std::size_t g_max_epochs = 200;
// UDM_ONLY=hit,churn runs those workloads and skips the rest, which is what drawing one of them at
// fifty sizes needs; empty means all of them.
std::string g_only;

auto wanted(char const* what) -> bool {
    return g_only.empty() || g_only.find(what) != std::string::npos;
}

template <typename Tuple, typename F>
void one_point(char const* what, std::size_t n, std::size_t batch, acc_row& acc, F&& f) {
    auto bench = ankerl::nanobench::Bench();
    bench.batch(static_cast<double>(batch))
        .performanceCounters(false)
        .output(nullptr)
        .targetIntervalWidth(g_interval)
        .maxEpochs(g_max_epochs);
    auto const res = compare_all<Tuple>(bench, f, std::make_index_sequence<num_maps<Tuple>>{});
    auto ns = std::vector<double>(num_maps<Tuple>);
    for (std::size_t a = 0; a < ns.size(); ++a) {
        ns[a] = res[a].result.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9 / static_cast<double>(batch);
    }
    print_point<Tuple>(what, n, ns, acc);
}

// One octave, log-spaced: the i-th of n points is base * 2^(i/n), so the set always spans exactly
// one doubling however many points are asked for. Five is the default and is what a ratio is
// summarised over; a couple of dozen is what draws the sawtooth itself.
std::size_t g_points = 5;
// How many points to actually run. Normally the same as g_points, which is exactly one octave;
// UDM_SPAN runs a few more at the same spacing, so a chart of the sawtooth can show the far side of
// the doubling instead of stopping on top of it.
std::size_t g_span = 0;

auto octave_size(std::size_t base, std::size_t i) -> std::size_t {
    return static_cast<std::size_t>(
        static_cast<double>(base) * std::pow(2.0, static_cast<double>(i) / static_cast<double>(g_points)));
}

template <typename Key, typename Val>
void sweep(std::size_t base) {
    using Tuple = typename maps_for<Key, Val>::type;
    constexpr auto k = num_maps<Tuple>;
    auto seq = std::make_index_sequence<k>{};
    (void)seq;

    auto a_build = acc_row(k);
    auto a_hit = acc_row(k);
    auto a_half = acc_row(k);
    auto a_miss = acc_row(k);
    auto a_iter = acc_row(k);
    auto a_churn = acc_row(k);
    auto a_ie = acc_row(k);

    std::printf("== %s octave starting at %zu ==\n", g_keyname, base);
    for (std::size_t i = 0; i < (g_span != 0 ? g_span : g_points); ++i) {
        auto const n = octave_size(base, i);
        auto const p = pools<Key>(n);
        auto const lookup_batch = std::size_t{20000};

        if (wanted("build")) {
        one_point<Tuple>("build", n, n, a_build, [&](auto ic) {
            return [&] {
                ankerl::nanobench::doNotOptimizeAway(build<std::tuple_element_t<ic.value, Tuple>>(p));
            };
        });
        }

        if (wanted("hit") || wanted("miss") || wanted("half") || wanted("iterate")) {
            auto ms = Tuple();
            std::apply([&](auto&... m) { (fill(m, p), ...); }, ms);
            auto st = std::vector<lookup_state>(k);
            for (auto const what : {asking::hits, asking::misses, asking::half}) {
                auto* acc = what == asking::hits ? &a_hit : (what == asking::misses ? &a_miss : &a_half);
                char const* label = what == asking::hits ? "hit" : (what == asking::misses ? "miss" : "half");
                if (!wanted(label)) {
                    continue;
                }
                one_point<Tuple>(label, n, lookup_batch, *acc, [&](auto ic) {
                    return [&, what] {
                        ankerl::nanobench::doNotOptimizeAway(
                            lookups(std::get<ic.value>(ms), p, st[ic.value], what, lookup_batch));
                    };
                });
            }
            if (wanted("iterate")) {
                one_point<Tuple>("iterate", n, n, a_iter, [&](auto ic) {
                    return [&] { ankerl::nanobench::doNotOptimizeAway(std::get<ic.value>(ms).sum()); };
                });
            }
        }

        if (wanted("churn")) {
            auto ms = Tuple();
            std::apply([&](auto&... m) { (fill(m, p), ...); }, ms);
            auto present = std::vector<std::vector<Key>>(k, p.present);
            auto spare = std::vector<std::vector<Key>>(k, p.spare);
            auto rngs = std::vector<ankerl::nanobench::Rng>();
            rngs.reserve(k);
            for (std::size_t a = 0; a < k; ++a) {
                rngs.emplace_back(7);
            }
            auto ticks = std::vector<std::size_t>(k, 0);
            auto const batch = std::size_t{20000};
            one_point<Tuple>("churn", n, batch, a_churn, [&](auto ic) {
                return [&] {
                    churn(std::get<ic.value>(ms), present[ic.value], spare[ic.value], rngs[ic.value], batch,
                          ticks[ic.value]);
                };
            });
        }

        if (wanted("ie")) {
            auto ms = Tuple();
            std::apply([&](auto&... m) { (fill(m, p), ...); }, ms);
            auto ps = std::vector<pools<Key>>(k, p);
            auto rngs = std::vector<ankerl::nanobench::Rng>();
            rngs.reserve(k);
            for (std::size_t a = 0; a < k; ++a) {
                rngs.emplace_back(7);
            }
            auto ticks = std::vector<std::size_t>(k, 0);
            auto const batch = std::size_t{10000};
            one_point<Tuple>("ie", n, 2 * batch, a_ie, [&](auto ic) {
                return [&] {
                    ankerl::nanobench::doNotOptimizeAway(insert_erase(std::get<ic.value>(ms), ps[ic.value],
                                                                      rngs[ic.value], batch, ticks[ic.value]));
                };
            });
        }
    }
    print_geomean<Tuple>("build", base, a_build);
    print_geomean<Tuple>("hit", base, a_hit);
    print_geomean<Tuple>("miss", base, a_miss);
    print_geomean<Tuple>("half", base, a_half);
    print_geomean<Tuple>("iterate", base, a_iter);
    print_geomean<Tuple>("churn", base, a_churn);
    print_geomean<Tuple>("ie", base, a_ie);
}

// ------------------------------------------------------------------ memory
#if defined(__GLIBC__)
auto heap_bytes() -> std::size_t {
    auto const mi = mallinfo2();
    return mi.uordblks + mi.hblkhd;
}

template <typename Map, typename Key>
void measure_memory(pools<Key> const& p, double& steady_out, double& churned_out) {
    // The churn's own key vectors are allocated before the snapshot; counting them inside it would
    // add eight bytes an entry to every map and hide what is being asked.
    auto present = p.present;
    auto spare = p.spare;
    auto const before = heap_bytes();
    {
        auto m = Map();
        fill(m, p);
        auto const steady = heap_bytes();
        auto rng = ankerl::nanobench::Rng(7);
        auto tick = std::size_t{0};
        churn(m, present, spare, rng, p.present.size(), tick);
        auto const churned = heap_bytes();
        steady_out = static_cast<double>(steady - before) / static_cast<double>(p.present.size());
        churned_out = static_cast<double>(churned - before) / static_cast<double>(p.present.size());
    }
}

template <typename Key, typename Val>
void memory(std::size_t base) {
    using Tuple = typename maps_for<Key, Val>::type;
    constexpr auto k = num_maps<Tuple>;
    static auto const names = names_of<Tuple>(std::make_index_sequence<k>{});
    auto ls = std::vector<double>(k, 0.0);
    auto lc = std::vector<double>(k, 0.0);
    for (std::size_t i = 0; i < g_points; ++i) {
        auto const p = pools<Key>(octave_size(base, i));
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (
                [&] {
                    double s = 0;
                    double c = 0;
                    measure_memory<std::tuple_element_t<I, Tuple>>(p, s, c);
                    ls[I] += std::log(s);
                    lc[I] += std::log(c);
                }(),
                ...);
        }(std::make_index_sequence<k>{});
    }
    std::printf("# bytes per entry, %s key, 8 byte value, octave geomean from n=%zu\n", g_keyname, base);
    std::printf("%-12s %10s %10s\n", "map", "steady", "churned");
    for (std::size_t a = 0; a < k; ++a) {
        auto const s = std::exp(ls[a] / static_cast<double>(g_points));
        auto const c = std::exp(lc[a] / static_cast<double>(g_points));
        std::printf("%-12s %10.1f %10.1f\n", names[a], s, c);
        if (g_csv != nullptr) {
            std::fprintf(g_csv, "%s,memory,%zu,%s,%.4f,%.4f\n", g_keyname, base, names[a], s, c);
        }
    }
    std::fflush(stdout);
}
#endif

// ------------------------------------------------------------------ correctness
// Every adapter has to agree with the group index operation for operation before any timing means
// anything. This is what caught Verstable's _insert being insert_or_assign rather than try_emplace.
template <typename Key, typename Val>
auto cross_check() -> int {
    using Tuple = typename maps_for<Key, Val>::type;
    constexpr auto k = num_maps<Tuple>;
    static auto const names = names_of<Tuple>(std::make_index_sequence<k>{});
    auto ms = Tuple();
    auto rng = ankerl::nanobench::Rng(1234);
    constexpr std::size_t ops = 400000;
    constexpr std::uint64_t range = 30000;
    auto bad = 0;

    auto each = [&](auto&& fn) {
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            (fn(std::integral_constant<std::size_t, I>{}, std::get<I>(ms)), ...);
        }(std::make_index_sequence<k>{});
    };

    for (std::size_t i = 0; i < ops && bad == 0; ++i) {
        auto const key = make_key<Key>(rng() % range);
        switch (rng() % 4) {
        case 0:
            each([&](auto, auto& m) { m.insert(key, 1); });
            break;
        case 1: {
            auto want = std::size_t{0};
            each([&](auto ic, auto& m) {
                auto const got = m.bump(key);
                if (ic.value == 0) {
                    want = got;
                } else if (got != want) {
                    std::printf("FAIL bump %s at %zu: %zu != %zu\n", names[ic.value], i, got, want);
                    bad = 1;
                }
            });
            break;
        }
        case 2:
            each([&](auto, auto& m) { m.erase(key); });
            break;
        default: {
            auto want = std::size_t{0};
            each([&](auto ic, auto& m) {
                auto const got = m.count(key);
                if (ic.value == 0) {
                    want = got;
                } else if (got != want) {
                    std::printf("FAIL count %s at %zu: %zu != %zu\n", names[ic.value], i, got, want);
                    bad = 1;
                }
            });
            break;
        }
        }
        auto want = std::size_t{0};
        each([&](auto ic, auto& m) {
            auto const got = m.size();
            if (ic.value == 0) {
                want = got;
            } else if (got != want) {
                std::printf("FAIL size %s at %zu: %zu != %zu\n", names[ic.value], i, got, want);
                bad = 1;
            }
        });
    }
    if (bad != 0) {
        return 1;
    }
    auto want = std::size_t{0};
    each([&](auto ic, auto& m) {
        auto const got = m.sum();
        if (ic.value == 0) {
            want = got;
        } else if (got != want) {
            std::printf("FAIL sum %s: %zu != %zu\n", names[ic.value], got, want);
            bad = 1;
        }
    });
    if (bad != 0) {
        return 1;
    }
    std::printf("ok %s: %zu maps agree over %zu operations, %zu entries, checksum %zu\n", g_keyname, k, ops,
                std::get<0>(ms).size(), want);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    workloads::tame_allocator();

    auto const mode = std::string(argc > 1 ? argv[1] : "speed");
    auto const keys = std::string(argc > 2 ? argv[2] : "u64");
    auto const base = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 0;
    if (char const* e = std::getenv("UDM_POINTS")) {
        g_points = std::strtoull(e, nullptr, 10);
    }
    if (char const* e = std::getenv("UDM_INTERVAL")) {
        g_interval = std::strtod(e, nullptr);
    }
    if (char const* e = std::getenv("UDM_EPOCHS")) {
        g_max_epochs = std::strtoull(e, nullptr, 10);
    }
    if (char const* e = std::getenv("UDM_SPAN")) {
        g_span = std::strtoull(e, nullptr, 10);
    }
    if (char const* e = std::getenv("UDM_ONLY")) {
        g_only = e;
    }
    if (char const* e = std::getenv("UDM_CSV")) {
        g_csv = std::fopen(e, "ae");
    }
    // "big" is the uint64_t key again, with a 64 byte mapped value: every cost of a flat map scales
    // with sizeof(value_type) and every cost of a dense one does not, and eight bytes of payload
    // hides that completely.
    g_keyname = keys.c_str();

    auto run = [&](auto tag, auto vtag) {
        using Key = decltype(tag);
        using Val = decltype(vtag);
        if (mode == "names") {
            // what scripts/ab/maps_one.sh asks for, so an index into the list has a name
            using Tuple = typename maps_for<Key, Val>::type;
            for (auto const* n : names_of<Tuple>(std::make_index_sequence<num_maps<Tuple>>{})) {
                std::printf("%s\n", n);
            }
            return 0;
        }
        if (mode == "check") {
            return cross_check<Key, Val>();
        }
#if defined(__GLIBC__)
        if (mode == "memory") {
            if (base != 0) {
                memory<Key, Val>(base);
                return 0;
            }
            for (auto b : {std::size_t{1000}, std::size_t{32000}, std::size_t{500000}}) {
                memory<Key, Val>(b);
            }
            return 0;
        }
#endif
        // A base of 0 means the three octaves the comparison is reported at; anything else is the
        // one octave asked for, which is how a single workload gets swept finely for a chart.
        if (base != 0) {
            sweep<Key, Val>(base);
            return 0;
        }
        for (auto b : {std::size_t{1000}, std::size_t{32000}, std::size_t{500000}}) {
            sweep<Key, Val>(b);
        }
        return 0;
    };

    auto const rc = keys == "str"  ? run(std::string{}, std::size_t{})
                    : keys == "big" ? run(std::uint64_t{}, workloads::big_value{})
                                    : run(std::uint64_t{}, std::size_t{});
    if (g_csv != nullptr) {
        std::fclose(g_csv);
    }
    return rc;
}
