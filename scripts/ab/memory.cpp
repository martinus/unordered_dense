// Bytes per entry against table size: what the map costs to hold, steady and at its peak.
//
// Speed alone picks the wrong map often enough to be worth a chart of its own, and the peak is a
// separate number from the steady state because growth allocates the new array beside the old one
// and only then frees it. That transient is what a caller sees as a spike, and for a segmented map
// -- whose values grow smoothly but whose index still doubles -- it is the whole of what segmenting
// does not buy.
//
// Total bytes, which is what a caller has to find room for. Bytes per entry is in the CSV beside it,
// since it is the better view of the *sawtooth* -- it is flat in size where the total is exponential
// -- but the question this chart answers is how much memory the map costs, and that is a total.
//
// No timing here, so it needs no pinning and no pairing: an allocator that counts is exact.
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
#include <third-party/nanobench.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <utility>
#include <vector>

namespace {

// One global pair of counters, because a container copies and rebinds its allocator freely and a
// per-object counter would follow the copies rather than the container.
std::size_t g_live = 0;
std::size_t g_peak = 0;

void reset_counters() {
    g_live = 0;
    g_peak = 0;
}

template <typename T>
struct counting {
    using value_type = T;

    counting() = default;
    // Not explicit: a container rebinds its allocator to other types and converts implicitly.
    template <typename U>
    counting(counting<U> const& /*other*/) noexcept {} // NOLINT(google-explicit-constructor)

    auto allocate(std::size_t n) -> T* {
        g_live += n * sizeof(T);
        g_peak = g_live > g_peak ? g_live : g_peak;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    void deallocate(T* p, std::size_t n) noexcept {
        g_live -= n * sizeof(T);
        ::operator delete(p);
    }
    template <typename U>
    auto operator==(counting<U> const& /*other*/) const noexcept -> bool {
        return true;
    }
    template <typename U>
    auto operator!=(counting<U> const& /*other*/) const noexcept -> bool {
        return false;
    }
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

// Built through try_emplace with nothing reserved, which is the growth the peak is about.
template <typename Map>
void measure(char const* name, std::size_t n) {
    reset_counters();
    {
        auto m = Map();
        auto r = ankerl::nanobench::Rng(1);
        while (m.size() < n) {
            m.try_emplace((r() >> 1U) | 1U, std::size_t{1});
        }
        auto const per = static_cast<double>(n);
        auto const mb = 1024.0 * 1024.0;
        std::printf("%zu,%s,%.6f,%.6f,%.3f,%.3f\n",
                    n,
                    name,
                    static_cast<double>(g_live) / mb,
                    static_cast<double>(g_peak) / mb,
                    static_cast<double>(g_live) / per,
                    static_cast<double>(g_peak) / per);
    }
}

} // namespace

auto main(int argc, char** argv) -> int {
    auto const max_shift = argc > 1 ? static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10)) : 20U;
    auto const per_octave = argc > 2 ? static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)) : 12U;

    using V = std::size_t;
    std::printf("entries,map,steady,peak,steady_per_entry,peak_per_entry\n");
    for (auto n : sample_sizes(max_shift, per_octave)) {
        measure<udmbase::unordered_dense::map<std::uint64_t,
                                              V,
                                              udmbase::unordered_dense::hash<std::uint64_t>,
                                              std::equal_to<std::uint64_t>,
                                              counting<std::pair<std::uint64_t, V>>>>("main", n);
        measure<ankerl::unordered_dense::map<std::uint64_t,
                                             V,
                                             ankerl::unordered_dense::hash<std::uint64_t>,
                                             std::equal_to<std::uint64_t>,
                                             counting<std::pair<std::uint64_t, V>>>>("this", n);
#ifdef UDM_AB_HAVE_JAN
        measure<udmjan::unordered_dense::map<std::uint64_t,
                                             V,
                                             udmjan::unordered_dense::hash<std::uint64_t>,
                                             std::equal_to<std::uint64_t>,
                                             counting<std::pair<std::uint64_t, V>>>>("jan", n);
#endif
#ifdef UDM_AB_HAVE_BOOST
        measure<boost::unordered_flat_map<std::uint64_t,
                                          V,
                                          ankerl::unordered_dense::hash<std::uint64_t>,
                                          std::equal_to<std::uint64_t>,
                                          counting<std::pair<std::uint64_t, V>>>>("boost", n);
#endif
    }
    return 0;
}
