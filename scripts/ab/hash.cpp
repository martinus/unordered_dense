// What a string hash costs against the length of the key, in throughput and in latency.
//
// The scored suite has one hash workload, `hashstr`, and it reports a single number over a mix of
// lengths. That is the right summary and it hides the shape: a hash dispatches on length, so its
// cost is a staircase with a step wherever the implementation changes strategy, and a change that
// helps one range can hurt another without the mix moving at all.
//
// Two panels, because a hash has two costs and they can disagree completely. **Throughput** is what
// a loop that hashes many independent keys pays, and it is what most benchmarks report.
// **Latency** is what a map lookup pays: the hash's result is the address of the group to probe, so
// nothing after it can start until the whole chain of multiplies has resolved. Measured here by
// feeding one byte of the answer back into the next key, so no two hashes overlap. The distinction
// decided a real question: an AES-NI hash measured a quarter faster in throughput and half again
// slower in latency, which makes it faster on this chart's left panel and slower in every map.
//
// One length per point, so the length dispatch is perfectly predicted here where the scored
// workload's mixed keys make it cost 0.31 branch misses per hash. That is the price of having
// length as an axis; `hashstr` is the number that includes the dispatch.
//
// Emits CSV on stdout; scripts/ab/plot.py --panels draws it.
#include <ankerl/unordered_dense.h>
#include <base.h>
#ifdef UDM_AB_HAVE_JAN
#    include <base_jan.h>
#endif
#ifdef UDM_AB_HAVE_BOOST
#    include <boost/container_hash/hash.hpp>
#endif
#include <bench/workloads.h>
#include <third-party/nanobench.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

// Enough keys that no single one stays in a register or a store buffer, few enough that the bodies
// are cache-resident at every length on this axis: this is a measurement of hashing, not of the
// memory system, and the size charts next door are where the memory system belongs. At 1024 bytes
// these are 256 KB, at 16 bytes they live inside the std::string objects as they would in a real
// table.
constexpr std::size_t num_keys = 256;

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

// Independent hashes, nothing carried between them: the loop can have as many in flight as the
// machine has multipliers.
template <typename Hash>
auto throughput(std::vector<std::string> const& keys, Hash&& h) -> std::uint64_t {
    auto acc = std::uint64_t{};
    for (auto const& k : keys) {
        acc += h(k);
    }
    return acc;
}

// One chain: a byte of each answer goes into the next key, which every one of these hashes reads,
// so the next hash cannot begin until this one has finished. The store-to-load forwarding that adds
// is a constant of a few cycles and lands on every alternative alike.
template <typename Hash>
auto latency(std::vector<std::string>& keys, Hash&& h) -> std::uint64_t {
    auto x = std::uint64_t{};
    for (auto& k : keys) {
        k[0] = static_cast<char>(x);
        x = h(k);
    }
    return x;
}

struct point {
    double median;
    double low;
    double high;
};

// Every alternative at one length, interleaved round by round in one process, which is what makes
// machine drift cancel out of the comparison rather than land on whichever went first. The rounds
// come from asking for a precision rather than naming a count; see scripts/ab/README.md.
template <typename... Alternatives>
auto measure(double targetWidth, Alternatives&&... alternatives) -> std::map<std::string, point> {
    auto bench = ankerl::nanobench::Bench();
    bench.batch(static_cast<double>(num_keys))
        .performanceCounters(false)
        .output(nullptr)
        .targetIntervalWidth(targetWidth)
        .maxEpochs(200);
    auto const res = bench.compare(std::forward<Alternatives>(alternatives)...);
    auto out = std::map<std::string, point>();
    for (std::size_t a = 0; a < res.size(); ++a) { // a CompareResult indexes; it is not a range
        auto const& r = res[a].result;
        auto times = std::vector<double>();
        times.reserve(r.size());
        for (std::size_t i = 0; i < r.size(); ++i) {
            times.push_back(r.get(i, ankerl::nanobench::Result::Measure::elapsed) * 1e9 /
                            static_cast<double>(num_keys));
        }
        auto const interval = ankerl::nanobench::detail::medianInterval(std::move(times), 0.95);
        out[r.config().mBenchmarkName] = {r.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9 /
                                              static_cast<double>(num_keys),
                                          interval.first,
                                          interval.second};
    }
    return out;
}

void one_length(std::size_t len, double targetWidth, bool emit) {
    auto keys = keys_of_length(len);
    auto const& ck = keys;

    auto const t = measure(
        targetWidth,
        "main",
        [&] { ankerl::nanobench::doNotOptimizeAway(throughput(ck, [](std::string const& s) {
                  return udmbase::unordered_dense::detail::wyhash::hash(s.data(), s.size()); })); },
        "this",
        [&] { ankerl::nanobench::doNotOptimizeAway(throughput(ck, [](std::string const& s) {
                  return ankerl::unordered_dense::detail::wyhash::hash(s.data(), s.size()); })); }
#ifdef UDM_AB_HAVE_JAN
        ,
        "jan",
        [&] { ankerl::nanobench::doNotOptimizeAway(throughput(ck, [](std::string const& s) {
                  return udmjan::unordered_dense::detail::wyhash::hash(s.data(), s.size()); })); }
#endif
#ifdef UDM_AB_HAVE_BOOST
        ,
        "boostdef",
        [&] { ankerl::nanobench::doNotOptimizeAway(throughput(ck, [](std::string const& s) {
                  return boost::hash<std::string>{}(s); })); }
#endif
    );

    auto const l = measure(
        targetWidth,
        "main",
        [&] { ankerl::nanobench::doNotOptimizeAway(latency(keys, [](std::string const& s) {
                  return udmbase::unordered_dense::detail::wyhash::hash(s.data(), s.size()); })); },
        "this",
        [&] { ankerl::nanobench::doNotOptimizeAway(latency(keys, [](std::string const& s) {
                  return ankerl::unordered_dense::detail::wyhash::hash(s.data(), s.size()); })); }
#ifdef UDM_AB_HAVE_JAN
        ,
        "jan",
        [&] { ankerl::nanobench::doNotOptimizeAway(latency(keys, [](std::string const& s) {
                  return udmjan::unordered_dense::detail::wyhash::hash(s.data(), s.size()); })); }
#endif
#ifdef UDM_AB_HAVE_BOOST
        ,
        "boostdef",
        [&] { ankerl::nanobench::doNotOptimizeAway(latency(keys, [](std::string const& s) {
                  return boost::hash<std::string>{}(s); })); }
#endif
    );

    if (!emit) {
        return; // the warm-up point: run for the machine's sake, report nothing
    }
    for (auto const& entry : t) {
        auto const found = l.find(entry.first);
        if (found == l.end()) {
            continue;
        }
        std::printf("%zu,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                    len,
                    entry.first.c_str(),
                    entry.second.median,
                    entry.second.low,
                    entry.second.high,
                    found->second.median,
                    found->second.low,
                    found->second.high);
    }
    std::fflush(stdout);
}

// Every byte through the short path and the independent blocks, so each step of the staircase is
// resolved; then coarser, since past 160 bytes the chained lanes make the curve a straight line
// with one kink at 192.
auto sample_lengths(std::size_t max_len) -> std::vector<std::size_t> {
    auto out = std::vector<std::size_t>();
    for (std::size_t n = 1; n <= (std::min)(max_len, std::size_t{160}); ++n) {
        out.push_back(n);
    }
    for (std::size_t n = 168; n <= max_len; n += 8) {
        out.push_back(n);
        if (n >= 256) {
            break;
        }
    }
    for (auto n = std::size_t{288}; n <= max_len; n = n * 5 / 4) {
        out.push_back(n);
    }
    return out;
}

} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    auto const max_len = argc > 1 ? std::strtoul(argv[1], nullptr, 10) : 1024UL;
    auto const targetWidth = argc > 2 ? std::strtod(argv[2], nullptr) : 0.03;

    // `entries` is the x column plot.py reads, and here it carries the key length.
    std::printf("entries,map,throughput,throughput_low,throughput_high,latency,latency_low,latency_high\n");
    auto const lengths = sample_lengths(max_len);
    // The first point of a process reads high -- cold caches, a cold allocator, a ramping clock --
    // and pairing cannot cancel it, because what is cold is the point rather than one alternative.
    one_length(lengths.front(), targetWidth, false);
    for (auto len : lengths) {
        one_length(len, targetWidth, true);
    }
    return 0;
}
