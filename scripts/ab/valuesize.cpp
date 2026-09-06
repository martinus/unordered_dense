// Build and iteration cost against the size of the mapped value.
//
// This is the axis that decides dense against flat, and it inverts along it: a flat map writes the
// whole value_type into a hash-scattered slot, so every cost it has scales with sizeof(value_type),
// where a dense map writes eight bytes there and appends the payload to a vector in order. Measured
// on a build with the same uint64_t key, boost::unordered_flat_map is ahead at a 16 byte value and
// behind at 64 -- so a suite that fixes the mapped type at size_t, which this one did until
// 2026-09-03, ranks dense and flat maps wrongly for map<Key, SomeStruct>.
//
// Two measurements per point, because they are the two halves of the same property: building the
// map from empty with nothing reserved, and one pass of iteration summing a byte of every value.
//
// The value type is trivially copyable on purpose. The variable under test is the *size* of the
// value, and an owned allocation would confound it with the heap.
//
// Emits CSV on stdout; scripts/ab/plot.py --panels draws it.
#include <ankerl/unordered_dense.h>
#include <base.h>
#ifdef UDM_AB_HAVE_JAN
#    include <base_jan.h>
#endif
#ifdef UDM_AB_HAVE_BOOST
#    include <boost/unordered/unordered_flat_map.hpp>
#endif
#include <bench/workloads.h>
#include <third-party/nanobench.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

template <std::size_t Bytes>
struct payload {
    std::uint64_t first{};
    std::array<std::uint8_t, Bytes - sizeof(std::uint64_t)> rest{};
};
template <>
struct payload<8> {
    std::uint64_t first{};
};

// The keys, made once and shared by every map and every value size, so that what differs between
// points is the value and nothing else.
auto keys_for(std::size_t n) -> std::vector<std::uint64_t> const& {
    static auto cache = std::map<std::size_t, std::vector<std::uint64_t>>();
    auto found = cache.find(n);
    if (found != cache.end()) {
        return found->second;
    }
    auto keys = std::vector<std::uint64_t>();
    auto r = ankerl::nanobench::Rng(1);
    keys.reserve(n);
    while (keys.size() < n) {
        keys.push_back((r() >> 1U) | 1U);
    }
    return cache.emplace(n, std::move(keys)).first->second;
}

template <typename Map>
auto build_map(std::vector<std::uint64_t> const& keys) -> Map {
    auto m = Map();
    for (auto k : keys) {
        m.try_emplace(k);
    }
    return m;
}

template <typename Map>
auto iterate(Map const& m) -> std::uint64_t {
    auto acc = std::uint64_t{};
    for (auto const& kv : m) {
        acc += kv.second.first;
    }
    return acc;
}

// One row per alternative, with both measurements on it. The two are timed in separate compare()
// calls -- interleaved within each, which is what makes drift cancel -- and joined here by name.
template <typename... Alternatives>
void emit(std::size_t bytes, char const* what, double batch, double targetWidth, Alternatives&&... alternatives) {
    auto bench = ankerl::nanobench::Bench();
    bench.batch(batch).performanceCounters(false).output(nullptr).targetIntervalWidth(targetWidth).maxEpochs(200);
    auto const res = bench.compare(std::forward<Alternatives>(alternatives)...);
    for (std::size_t i = 0; i < res.size(); ++i) {
        std::printf("%zu,%s,%s,%.4f\n",
                    bytes,
                    res[i].result.config().mBenchmarkName.c_str(),
                    what,
                    res[i].result.median(ankerl::nanobench::Result::Measure::elapsed) * 1e9 / batch);
    }
}

template <std::size_t Bytes>
void one_size(std::size_t n, double targetWidth) {
    using V = payload<Bytes>;
    using main_map = udmbase::unordered_dense::map<std::uint64_t, V>;
    using this_map = ankerl::unordered_dense::map<std::uint64_t, V>;
#ifdef UDM_AB_HAVE_JAN
    using jan_map = udmjan::unordered_dense::map<std::uint64_t, V>;
#endif
#ifdef UDM_AB_HAVE_BOOST
    using boost_map = boost::unordered_flat_map<std::uint64_t, V, ankerl::unordered_dense::hash<std::uint64_t>>;
#endif
    auto const& keys = keys_for(n);
    auto const per = static_cast<double>(n);

    emit(Bytes,
         "build",
         per,
         targetWidth,
         "main",
         [&] { ankerl::nanobench::doNotOptimizeAway(build_map<main_map>(keys).size()); },
         "this",
         [&] { ankerl::nanobench::doNotOptimizeAway(build_map<this_map>(keys).size()); }
#ifdef UDM_AB_HAVE_JAN
         ,
         "jan",
         [&] { ankerl::nanobench::doNotOptimizeAway(build_map<jan_map>(keys).size()); }
#endif
#ifdef UDM_AB_HAVE_BOOST
         ,
         "boost",
         [&] { ankerl::nanobench::doNotOptimizeAway(build_map<boost_map>(keys).size()); }
#endif
    );

    // Built once and iterated many times, which is what iteration is for; building inside the timed
    // region would measure the build again and swamp it.
    auto m0 = build_map<main_map>(keys);
    auto m1 = build_map<this_map>(keys);
#ifdef UDM_AB_HAVE_JAN
    auto m3 = build_map<jan_map>(keys);
#endif
#ifdef UDM_AB_HAVE_BOOST
    auto m2 = build_map<boost_map>(keys);
#endif
    emit(Bytes,
         "iterate",
         per,
         targetWidth,
         "main",
         [&] { ankerl::nanobench::doNotOptimizeAway(iterate(m0)); },
         "this",
         [&] { ankerl::nanobench::doNotOptimizeAway(iterate(m1)); }
#ifdef UDM_AB_HAVE_JAN
         ,
         "jan",
         [&] { ankerl::nanobench::doNotOptimizeAway(iterate(m3)); }
#endif
#ifdef UDM_AB_HAVE_BOOST
         ,
         "boost",
         [&] { ankerl::nanobench::doNotOptimizeAway(iterate(m2)); }
#endif
    );
}

} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    auto const n = argc > 1 ? std::strtoul(argv[1], nullptr, 10) : 200000UL;
    auto const targetWidth = argc > 2 ? std::strtod(argv[2], nullptr) : 0.02;

    std::printf("entries,map,what,ns\n");
    // `entries` is the x axis the plotter reads, and here it carries the value size instead.
    //
    // It stops at 64 bytes, and that is a measurement decision rather than a lack of curiosity: at
    // 200000 entries a 64 byte value is 14 MB of values, which still fits this machine's L3, and 128
    // is 27 MB, which does not. Sampled past that, every line bends upward together and the chart
    // stops being about the value size -- measured, boost against this map on a build goes 1.34,
    // 1.34, 1.46, 2.06 across 8 to 64 bytes and then 0.96 and 1.09 at 128 and 256, which is the
    // cache cliff talking and not the layout. The out-of-cache regime is what the size-axis charts
    // are for; this one holds the working set still and moves only the value.
    one_size<8>(n, targetWidth);
    one_size<16>(n, targetWidth);
    one_size<24>(n, targetWidth);
    one_size<32>(n, targetWidth);
    one_size<48>(n, targetWidth);
    one_size<64>(n, targetWidth);
    return 0;
}
