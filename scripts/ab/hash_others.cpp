// What the string hash of every map in the comparison costs, in latency and in throughput.
//
// scripts/ab/hash.cpp charts this library's hash against its own older versions over a length axis.
// This one asks the other question: against the hash each *other* library ships, at the lengths the
// scored suite uses. The two costs are measured separately and they order the candidates
// differently, which is the whole point:
//
//   throughput -- independent keys, as many in flight as the machine has multipliers. What a loop
//                 that hashes a column of data pays, and what a hash benchmark usually reports.
//   latency    -- one chain: a byte of each answer is written into the next key before it is
//                 hashed, so no two hashes overlap. What a *map lookup* pays, since the hash's
//                 result is the address of the group to probe and nothing after it can start.
//
// A "floor" row hashes nothing -- it xors the size with the first byte -- so it measures what the
// chain itself costs: the store into the key, the load back, and the loop. Every row carries that
// constant, so the differences between rows are the honest part and the absolute numbers are not.
//
// The chain is built by writing into the key rather than by choosing the next key with the answer
// (`x = hash(keys[x & mask])`), which is the obvious way and is wrong: it puts the key's *length*
// and *address* on the dependency chain, which a real lookup does not have -- the caller already
// holds the key. That harness reported a 1.40x for a change worth nothing in the map.
//
//   scripts/ab/hash_others.sh [-c compiler]
#include <ankerl/unordered_dense.h>
#ifdef UDM_AB_HAVE_BASE
#    include <base.h>
#endif
#ifdef UDM_AB_HAVE_BOOST
#    include <boost/container_hash/hash.hpp>
#endif
#ifdef UDM_AB_HAVE_ABSL
#    include <absl/hash/hash.h>
#endif
#ifdef UDM_AB_HAVE_FOLLY
#    include <folly/hash/Hash.h>
#endif
#ifdef UDM_AB_HAVE_RAPIDHASH
#    include <rapidhash.h>
#endif
#ifdef UDM_AB_HAVE_KOMIHASH
#    include <komihash.h>
#endif
#ifdef UDM_AB_HAVE_POLYMUR
#    include <polymur-hash.h>
#endif
#ifdef UDM_AB_HAVE_AQUAHASH
#    include <aquahash.h>
#endif
#include "hash_ports.h"
#include <bench/workloads.h>
#include <third-party/nanobench.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr std::size_t num_keys = 256;

// Fixed-length keys, for the shape, and the scored suite's own mix, which is what the map
// benchmarks in this post actually hash: 8 to 135 bytes, skewed towards short, so the length
// dispatch is unpredictable the way it is in a real table.
auto keys_of_length(std::size_t len) -> std::vector<std::string> {
    auto out = std::vector<std::string>();
    out.reserve(num_keys);
    auto rng = ankerl::nanobench::Rng(len * 2654435761U + 1U);
    for (std::size_t i = 0; i < num_keys; ++i) {
        auto s = std::string(len, '\0');
        for (std::size_t j = 0; j < len; j += 8) {
            auto const v = rng();
            std::memcpy(s.data() + j, &v, (std::min)(std::size_t{8}, len - j));
        }
        out.push_back(std::move(s));
    }
    return out;
}

auto scored_keys() -> std::vector<std::string> {
    auto out = std::vector<std::string>();
    out.reserve(num_keys);
    auto rng = ankerl::nanobench::Rng(1234);
    for (std::size_t i = 0; i < num_keys; ++i) {
        out.push_back(workloads::key_source<std::string>::get(rng()));
    }
    return out;
}

template <typename Hash>
auto throughput(std::vector<std::string> const& keys, Hash&& h) -> std::uint64_t {
    auto acc = std::uint64_t{};
    for (auto const& k : keys) {
        acc += h(k);
    }
    return acc;
}

template <typename Hash>
auto latency(std::vector<std::string>& keys, Hash&& h) -> std::uint64_t {
    auto x = std::uint64_t{};
    for (auto& k : keys) {
        k[0] = static_cast<char>(x);
        x = h(k);
    }
    return x;
}

struct row {
    double thr;
    double lat;
};

// Every hasher interleaved round by round in one process, so machine drift cancels out of the
// comparison instead of landing on whichever ran first.
#define UDM_EACH_HASH(WRAP)                                                                                                 \
    "udm5", WRAP(ankerl::unordered_dense::detail::wyhash::hash(s.data(), s.size())), "floor",                               \
        WRAP(static_cast<std::uint64_t>(s.size()) ^ static_cast<std::uint64_t>(s[0])) UDM_BASE(WRAP) UDM_BOOST(WRAP)        \
            UDM_ABSL(WRAP) UDM_FOLLY(WRAP),                                                                                 \
        "foldhash", WRAP(udm_ab::foldhash::fast(s.data(), s.size())), "foldhash-q",                                         \
        WRAP(udm_ab::foldhash::quality(s.data(), s.size())) UDM_RAPID(WRAP) UDM_KOMI(WRAP) UDM_POLYMUR(WRAP) UDM_AQUA(WRAP) \
            UDM_GX(WRAP)

#ifdef UDM_AB_HAVE_BASE
#    define UDM_BASE(WRAP) , "udm4", WRAP(udmbase::unordered_dense::detail::wyhash::hash(s.data(), s.size()))
#else
#    define UDM_BASE(WRAP)
#endif
#ifdef UDM_AB_HAVE_BOOST
#    define UDM_BOOST(WRAP) , "boost", WRAP(boost::hash<std::string>{}(s))
#else
#    define UDM_BOOST(WRAP)
#endif
#ifdef UDM_AB_HAVE_ABSL
#    define UDM_ABSL(WRAP) , "absl", WRAP(absl::Hash<std::string>{}(s))
#else
#    define UDM_ABSL(WRAP)
#endif
#ifdef UDM_AB_HAVE_FOLLY
#    define UDM_FOLLY(WRAP) , "folly", WRAP(folly::hasher<std::string>{}(s))
#else
#    define UDM_FOLLY(WRAP)
#endif
#ifdef UDM_AB_HAVE_RAPIDHASH
#    define UDM_RAPID(WRAP) , "rapidNano", WRAP(rapidhashNano(s.data(), s.size()))
#else
#    define UDM_RAPID(WRAP)
#endif
#ifdef UDM_AB_HAVE_KOMIHASH
#    define UDM_KOMI(WRAP) , "komihash", WRAP(komihash(s.data(), s.size(), UINT64_C(0x1234567890abcdef)))
#else
#    define UDM_KOMI(WRAP)
#endif
#ifdef UDM_AB_HAVE_POLYMUR
// polymur derives a key schedule from a seed once, so the params are a translation-unit constant
// rather than something the hashed expression builds.
inline auto const polymur_params = [] {
    auto p = PolymurHashParams();
    polymur_init_params_from_seed(&p, UINT64_C(0xfedcba9876543210));
    return p;
}();
#    define UDM_POLYMUR(WRAP) \
        , "polymur", WRAP(polymur_hash(reinterpret_cast<std::uint8_t const*>(s.data()), s.size(), &polymur_params, 0))
#else
#    define UDM_POLYMUR(WRAP)
#endif
#if defined(UDM_AB_HAVE_AQUAHASH) && defined(__AES__)
#    define UDM_AQUA(WRAP)                   \
        , "AquaHash",                        \
            WRAP(static_cast<std::uint64_t>( \
                _mm_cvtsi128_si64(AquaHash::Hash(reinterpret_cast<std::uint8_t const*>(s.data()), s.size()))))
#else
#    define UDM_AQUA(WRAP)
#endif
#if defined(__AES__)
#    define UDM_GX(WRAP) , "gxhash", WRAP(udm_ab::gxhash::hash(s.data(), s.size()))
#else
#    define UDM_GX(WRAP)
#endif

#define UDM_THROUGHPUT(EXPR)                                                           \
    [&] {                                                                              \
        ankerl::nanobench::doNotOptimizeAway(throughput(ck, [](std::string const& s) { \
            return EXPR;                                                               \
        }));                                                                           \
    }
#define UDM_LATENCY(EXPR)                                                             \
    [&] {                                                                             \
        ankerl::nanobench::doNotOptimizeAway(latency(keys, [](std::string const& s) { \
            return EXPR;                                                              \
        }));                                                                          \
    }

auto measure(std::vector<std::string>& keys, double width) -> std::map<std::string, row> {
    auto const& ck = keys;
    auto bench = [&] {
        auto b = ankerl::nanobench::Bench();
        // A long warm-up and a minimum epoch of a millisecond: the first iterations of a length
        // pay for a cold key buffer and a clock that has not ramped, and an epoch shorter than a
        // scheduler tick measures the scheduler. targetIntervalWidth asks for a precision rather
        // than naming a round count, so a noisy length simply runs longer than a quiet one.
        b.batch(static_cast<double>(num_keys))
            .performanceCounters(false)
            .output(nullptr)
            .warmup(200)
            .minEpochTime(std::chrono::milliseconds(1))
            .targetIntervalWidth(width)
            .maxEpochs(500);
        return b;
    };
    auto out = std::map<std::string, row>();
    auto collect = [&](auto const& res, bool is_latency) {
        for (std::size_t a = 0; a < res.size(); ++a) {
            auto const& r = res[a].result;
            auto const ns = r.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9 / static_cast<double>(num_keys);
            auto& slot = out[r.config().mBenchmarkName];
            (is_latency ? slot.lat : slot.thr) = ns;
        }
    };
    auto bt = bench();
    collect(bt.compare(UDM_EACH_HASH(UDM_THROUGHPUT)), false);
    auto bl = bench();
    collect(bl.compare(UDM_EACH_HASH(UDM_LATENCY)), true);
    return out;
}

void report(char const* label, std::vector<std::string>& keys, double width, bool emit) {
    auto const r = measure(keys, width);
    if (!emit) {
        return; // warm-up: the first measurement of a process pays for cold caches and a cold clock
    }
    for (auto const& [name, v] : r) {
        std::printf("%s,%s,%.3f,%.3f\n", label, name.c_str(), v.thr, v.lat);
    }
    std::fflush(stdout);
}

// The lengths the sweep samples: every byte through the short path and the first independent
// blocks, where each change of strategy is one step, then coarsening, since past 160 bytes the
// chained lanes make every curve a straight line on a log axis.
auto sweep_lengths() -> std::vector<std::size_t> {
    auto out = std::vector<std::size_t>();
    for (std::size_t n = 4; n <= 72; ++n) {
        out.push_back(n);
    }
    for (std::size_t n = 76; n <= 160; n += 4) {
        out.push_back(n);
    }
    for (std::size_t n = 176; n <= 320; n += 16) {
        out.push_back(n);
    }
    for (std::size_t n = 352; n <= 1024; n += 32) {
        out.push_back(n);
    }
    return out;
}

} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    auto const mode = std::string(argc > 1 ? argv[1] : "table");
    auto const width = argc > 2 ? std::atof(argv[2]) : 0.02;

    auto warm = keys_of_length(32);
    report("warmup", warm, width, false);

    if (mode == "sweep") {
        // One line per length per hash, for the latency chart. Throughput is measured anyway --
        // the two share a set of keys and the second measurement is the cheaper half of the run.
        std::printf("bytes,hash,throughput_ns,latency_ns\n");
        for (auto len : sweep_lengths()) {
            auto keys = keys_of_length(len);
            report(std::to_string(len).c_str(), keys, width, true);
        }
        return 0;
    }

    std::printf("keys,hash,throughput_ns,latency_ns\n");
    for (auto len : {8U, 16U, 32U, 64U, 128U, 256U}) {
        auto keys = keys_of_length(len);
        report((std::to_string(len) + "B").c_str(), keys, width, true);
    }
    auto mix = scored_keys();
    report("mix", mix, width, true);
    return 0;
}
