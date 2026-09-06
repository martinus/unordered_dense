// Megabytes held against table size or against mapped-value size, steady and at the growth peak.
//
// Speed alone picks the wrong map often enough to be worth measuring, and the peak is a separate
// number from the steady state because growth allocates the new array beside the old one and only
// then frees it. That transient is what a caller has to have room for.
//
// Every allocation the process makes is counted, by replacing global new and delete rather than by
// handing the container an allocator. With `std::string` keys that is the difference between
// measuring the map and measuring a third of it: the key bodies are allocated by
// `std::allocator<char>` inside each string, which a container's allocator never sees, so an
// allocator-based count reports a string map as costing what its 32 byte string headers cost. For
// integer keys, where nothing is hidden, the two methods agree to the byte.
//
// No timing here, so it needs no pinning and no pairing: a count is exact.
//
// Emits CSV on stdout; scripts/ab/plot.py --panels --bars draws it.
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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

namespace {
std::size_t g_live = 0;
std::size_t g_peak = 0;
// Only what the map itself asks for: the keys are built before this goes true, so the vector holding
// them is not charged to the map it fills.
bool g_counting = false;

// The mapped values the value-size axis walks. Trivially copyable on purpose: the variable under
// test is the size, and an owned allocation would confound it with the heap.
template <std::size_t Bytes>
struct payload {
    std::array<std::uint8_t, Bytes> data{};
};

auto sample_sizes(unsigned max_shift, unsigned per_octave) -> std::vector<std::size_t> {
    auto out = std::vector<std::size_t>();
    for (auto i = 0U;; ++i) {
        auto const n = static_cast<std::size_t>(16.0 * std::pow(2.0, static_cast<double>(i) / per_octave) + 0.5);
        if (n > (std::size_t{1} << max_shift)) {
            break;
        }
        if (out.empty() || out.back() != n) {
            out.push_back(n);
        }
    }
    return out;
}

// Built through try_emplace with nothing reserved, which is the growth the peak is about. `x` is
// what the row is indexed by: the entry count on the size axis, the value size on the other.
template <typename Map>
void measure(char const* name, std::size_t n, std::size_t x) {
    auto keys = std::vector<typename Map::key_type>();
    keys.reserve(n);
    auto r = ankerl::nanobench::Rng(1);
    while (keys.size() < n) {
        keys.emplace_back(workloads::key_for<Map>(r() >> 2U));
    }
    g_live = 0;
    g_peak = 0;
    g_counting = true;
    {
        auto m = Map();
        for (auto const& k : keys) {
            m.try_emplace(k);
        }
        g_counting = false;
        auto const per = static_cast<double>(n);
        auto const mb = 1024.0 * 1024.0;
        std::printf("%zu,%s,%.6f,%.6f,%.3f,%.3f\n",
                    x != 0 ? x : n,
                    name,
                    static_cast<double>(g_live) / mb,
                    static_cast<double>(g_peak) / mb,
                    static_cast<double>(g_live) / per,
                    static_cast<double>(g_peak) / per);
    }
    g_counting = false;
}

// All four maps at one point of whichever axis.
template <typename Key, typename V>
void one_point(std::size_t n, std::size_t x) {
    measure<udmbase::unordered_dense::map<Key, V>>("main", n, x);
    measure<ankerl::unordered_dense::map<Key, V>>("this", n, x);
#ifdef UDM_AB_HAVE_JAN
    measure<udmjan::unordered_dense::map<Key, V>>("jan", n, x);
#endif
#ifdef UDM_AB_HAVE_BOOST
    measure<boost::unordered_flat_map<Key, V, ankerl::unordered_dense::hash<Key>>>("boost", n, x);
    measure<boost::unordered_flat_map<Key, V>>("boostdef", n, x); // the hash boost ships with
#endif
}

template <typename Key>
void value_size_axis(std::size_t n) {
    one_point<Key, payload<8>>(n, 8);
    one_point<Key, payload<16>>(n, 16);
    one_point<Key, payload<24>>(n, 24);
    one_point<Key, payload<32>>(n, 32);
    one_point<Key, payload<48>>(n, 48);
    one_point<Key, payload<64>>(n, 64);
    one_point<Key, payload<96>>(n, 96);
    one_point<Key, payload<128>>(n, 128);
    one_point<Key, payload<192>>(n, 192);
    one_point<Key, payload<256>>(n, 256);
}

} // namespace

void* operator new(std::size_t n) {
    auto* p = std::malloc(n + 16);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    *static_cast<std::size_t*>(p) = n;
    if (g_counting) {
        g_live += n;
        g_peak = g_live > g_peak ? g_live : g_peak;
    }
    return static_cast<char*>(p) + 16;
}

void operator delete(void* p) noexcept {
    if (p == nullptr) {
        return;
    }
    auto* base = static_cast<char*>(p) - 16;
    if (g_counting) {
        g_live -= *reinterpret_cast<std::size_t*>(base);
    }
    std::free(base);
}

void operator delete(void* p, std::size_t /*n*/) noexcept {
    ::operator delete(p);
}

auto main(int argc, char** argv) -> int {
    auto const max_shift = argc > 1 ? static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10)) : 20U;
    auto const per_octave = argc > 2 ? static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)) : 12U;
    // 1 walks the mapped-value size at a fixed entry count instead of the entry count at a fixed
    // value size. That is the axis memory differentiates on: per entry it barely moves with the
    // table size, which is why the size chart is sixteen near-identical octaves.
    auto const mode = argc > 3 ? std::atoi(argv[3]) : 0;
    auto const entries = argc > 4 ? std::strtoul(argv[4], nullptr, 10) : 1000000UL;
    // 0 uint64_t keys, 1 std::string keys
    auto const key = argc > 5 ? std::atoi(argv[5]) : 0;

    std::printf("entries,map,steady,peak,steady_per_entry,peak_per_entry\n");
    if (mode == 1) {
        if (key == 1) {
            value_size_axis<std::string>(entries);
        } else {
            value_size_axis<std::uint64_t>(entries);
        }
        return 0;
    }
    for (auto n : sample_sizes(max_shift, per_octave)) {
        if (key == 1) {
            one_point<std::string, std::size_t>(n, 0);
        } else {
            one_point<std::uint64_t, std::size_t>(n, 0);
        }
    }
    return 0;
}
