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
#include <cstdio>
#include <cstdlib>
#include <vector>

unsigned long long udm_probe_groups = 0;
unsigned long long udm_probe_lookups = 0;

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
} // namespace

int main(int argc, char** argv) {
    auto const load = argc > 1 ? std::strtod(argv[1], nullptr) : 0.76;
    auto const turnovers = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 200;
    // 4096 groups = 65536 slots; fill to the requested load
    auto const slots = std::size_t{65536};
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
    auto next = std::uint64_t{1} << 40U;
    auto const hits = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 0;
    for (std::size_t t = 0; t < turnovers; ++t) {
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t h = 0; h < hits; ++h) {
                ++m[present[static_cast<std::size_t>(r() % present.size())]];
            }
            auto const at = static_cast<std::size_t>(r() % present.size());
            m.erase(present[at]);
            present[at] = next++;
            m.try_emplace(present[at], 1);
        }
    }
    auto const churn_hit = measure(m, present, 200000);
    auto const churn_miss = measure(m, absent, 200000);

    std::printf("load %.3f, %llu turnovers, %llu writing hits/round, %zu entries in %zu slots\n",
                static_cast<double>(m.size()) / static_cast<double>(m.bucket_count()),
                static_cast<unsigned long long>(turnovers), static_cast<unsigned long long>(hits), m.size(),
                m.bucket_count());
    std::printf("  groups per hit   fresh %.4f   churned %.4f\n", fresh_hit, churn_hit);
    std::printf("  groups per miss  fresh %.4f   churned %.4f\n", fresh_miss, churn_miss);
}
