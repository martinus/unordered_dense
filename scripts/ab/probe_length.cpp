// Groups visited per lookup, fresh and after churn.
//
// Because nothing moves after it is placed, an entry that landed away from home while its home
// group was full stays there once the home empties again, so a table that has churned probes a
// little further than a freshly built one with the same contents. This counts how much further, and
// what move_home() takes back -- pass a number of writing lookups per churn round, since that is
// the only path move_home() runs on.
//
// Built by scripts/ab/probe_length.sh, which is where the counter is patched into a copy of the
// probe; it does not belong in the header, where it would be in everybody's lookup.
#include UDM_INSTRUMENTED_HEADER
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

unsigned long long udm_probe_groups = 0;
unsigned long long udm_probe_lookups = 0;
unsigned long long udm_rebuilds = 0;
unsigned long long udm_pullbacks = 0;

namespace {
using map_t = ankerl::unordered_dense::map<std::uint64_t, std::uint64_t>;

struct rng {
    std::uint64_t s = 0x853c49e6748fea9bULL;
    auto operator()() -> std::uint64_t {
        s ^= s << 13U;
        s ^= s >> 7U;
        s ^= s << 17U;
        return s;
    }
};

auto measure(map_t const& m, std::vector<std::uint64_t> const& keys, std::size_t n) -> double {
    udm_probe_groups = 0;
    udm_probe_lookups = 0;
    auto acc = std::size_t{0};
    for (std::size_t i = 0; i < n; ++i) {
        acc += m.count(keys[i % keys.size()]);
    }
    if (acc == 12345678) {
        std::printf("x");
    }
    return static_cast<double>(udm_probe_groups) / static_cast<double>(udm_probe_lookups);
}
// Groups per miss if each group also had an exact in-home counter (#366): per class, how many
// entries homed in this group live outside it. A miss whose home has zero there stops at home even
// when the overflow counter, which also counts entries that only passed through, says go on.
// Computed offline from index() and values(); the walk past home is the real one over the real
// counters, which reproduced measure() exactly, and a header with the counter built read the same.
auto exact_counter_miss(map_t const& m, std::vector<std::uint64_t> const& keys, std::size_t n) -> double {
    auto const idx = m.index();
    auto const* blocks = idx.data();
    auto const num_groups = idx.size();
    auto const group_mask = num_groups - 1;
    auto shifts = 64U;
    for (auto g = num_groups; g > 1; g /= 2) {
        --shifts;
    }
    auto const& values = m.values();
    auto const hasher = m.hash_function();
    auto const class_of = [](std::uint64_t mh) {
        return ankerl::unordered_dense::detail::fingerprint_words[mh & 0xFFU] & 7U;
    };
    auto exact = std::vector<std::array<std::uint32_t, 8>>(num_groups);
    for (std::size_t g = 0; g < num_groups; ++g) {
        for (std::size_t l = 0; l < blocks[g].m_fingerprints.size(); ++l) {
            if (blocks[g].m_fingerprints[l] == 0) {
                continue;
            }
            auto const mh = hasher(values[blocks[g].m_index[l]].first);
            auto const home = static_cast<std::size_t>(mh >> shifts);
            if (home != g) {
                ++exact[home][class_of(mh)];
            }
        }
    }
    auto with_exact = std::size_t{0};
    for (std::size_t i = 0; i < n; ++i) {
        auto const mh = hasher(keys[i % keys.size()]);
        auto const c = class_of(mh);
        auto g = static_cast<std::size_t>(mh >> shifts);
        auto const home = g;
        auto walked = std::size_t{1};
        for (std::size_t delta = 1; blocks[g].m_overflows[c] != 0 && delta <= group_mask; ++delta) {
            g = (g + delta) & group_mask;
            ++walked;
        }
        with_exact += exact[home][c] == 0 ? 1 : walked;
    }
    return static_cast<double>(with_exact) / static_cast<double>(n);
}
} // namespace

int main(int argc, char** argv) {
    auto const load = argc > 1 ? std::strtod(argv[1], nullptr) : 0.76;
    // turnovers: one number, or ascending checkpoints separated by commas ("0.1,0.5,1,10"), each
    // measured on the same table as it keeps churning. Fractions are fine: 0.25 is n/4 rounds.
    auto checkpoints = std::vector<double>();
    {
        char const* p = argc > 2 ? argv[2] : "200";
        while (*p != '\0') {
            char* end = nullptr;
            checkpoints.push_back(std::strtod(p, &end));
            p = *end == ',' ? end + 1 : end;
        }
    }
    // 4096 groups = 65536 slots by default; fill to the requested load
    auto const groups = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 4096;
    auto const slots = static_cast<std::size_t>(groups) * 16U;
    auto const n = static_cast<std::size_t>(static_cast<double>(slots) * load);

    auto r = rng();
    auto present = std::vector<std::uint64_t>();
    auto absent = std::vector<std::uint64_t>();
    for (std::size_t i = 0; i < n; ++i) {
        present.push_back(r() >> 1U);
    }
    for (std::size_t i = 0; i < 20000; ++i) {
        absent.push_back(r() | (std::uint64_t{1} << 63U));
    }

    auto m = map_t();
    m.reserve(n);
    for (auto k : present) {
        m.try_emplace(k, 1);
    }
    auto const fresh_hit = measure(m, present, 200000);
    auto const fresh_miss = measure(m, absent, 200000);

    // churn at a fixed size, with a key the map has never held. `hits` writing lookups per round:
    // operator[] on a key that is present is the path move_home() runs on, so this is what says
    // what move_home takes back.
    auto const hits = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 0;
    std::printf("load %.3f, %llu writing hits/round, %zu entries in %zu slots\n",
                static_cast<double>(m.size()) / static_cast<double>(m.bucket_count()),
                static_cast<unsigned long long>(hits),
                m.size(),
                m.bucket_count());
    std::printf("  groups per hit   fresh %.4f\n", fresh_hit);
    std::printf("  groups per miss  fresh %.4f\n", fresh_miss);
    auto rounds_done = std::size_t{0};
    auto const churn_round = [&] {
        for (std::size_t h = 0; h < hits; ++h) {
            ++m[present[static_cast<std::size_t>(r() % present.size())]];
        }
        auto const at = static_cast<std::size_t>(r() % present.size());
        m.erase(present[at]);
        // A random key, like the ones the table was filled with. A counter here (it was
        // `next++` until 2026-10-02) leaves a churned table holding sequential keys,
        // which the hash spreads almost evenly: drift read 1.036/1.061 groups per
        // hit/miss instead of 1.136/1.265, and FIFO churn read exactly 1.000.
        present[at] = r() >> 1U;
        m.try_emplace(present[at], 1);
    };
    // the interval mean samples every 0.05 turnovers with fewer lookups, and only when there are
    // intervals to compare: a single checkpoint reads the table at its end, as it always did
    auto const sampling = checkpoints.size() > 1;
    auto const sample_every = n / 20 == 0 ? std::size_t{1} : n / 20;
    for (auto const turnovers : checkpoints) {
        auto const rounds = static_cast<std::size_t>(turnovers * static_cast<double>(n) + 0.5);
        auto const rebuilds_before = udm_rebuilds;
        auto const pullbacks_before = udm_pullbacks;
        auto sum_exact = 0.0;
        auto sum_hit = 0.0;
        auto sum_miss = 0.0;
        auto samples = std::size_t{0};
        for (; rounds_done < rounds; ++rounds_done) {
            churn_round();
            if (sampling && (rounds_done + 1) % sample_every == 0) {
                sum_hit += measure(m, present, 20000);
                sum_miss += measure(m, absent, 20000);
                sum_exact += exact_counter_miss(m, absent, 20000);
                ++samples;
            }
        }
        auto const churn_hit = measure(m, present, 200000);
        auto const churn_miss = measure(m, absent, 200000);
        std::printf("  %g turnovers: groups per hit churned %.4f, per miss churned %.4f", turnovers, churn_hit, churn_miss);
        std::printf("; exact counter: miss %.4f", exact_counter_miss(m, absent, 200000));
        if (samples != 0) {
            std::printf("; interval mean hit %.4f, miss %.4f, exact-counter miss %.4f, %llu rebuilds, %llu pullbacks",
                        sum_hit / static_cast<double>(samples),
                        sum_miss / static_cast<double>(samples),
                        sum_exact / static_cast<double>(samples),
                        udm_rebuilds - rebuilds_before,
                        udm_pullbacks - pullbacks_before);
        }
        std::printf("\n");
    }
}
