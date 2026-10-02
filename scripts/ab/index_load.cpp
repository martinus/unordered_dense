// Loading a map from its values and its index (#299): what each way of getting a usable table costs,
// against building it by inserting.
//
//   index_load <n> [rounds]
//
// One mode per binary (-DUDM_MODE=...), because two timed functions in one TU moved a pipelined side
// 11% once (notes: "The harness had to be pinned first"):
//
//   build           insert every key into a fresh map (reserved: the growth is not the question)
//   owning          map(std::move(values), index, trust::checked): the values move in, the index is
//                   copied and checked in the same loop
//   owning_unchecked  the same with trust::unchecked: the copy alone
//   view_checked    map_view over the bytes, trust::checked
//   view_unchecked  map_view over the bytes, trust::unchecked
//   verify_spot     verify(spot) on an owning table
//   verify_full     verify(full) on an owning table
//
// -DUDM_STR for map<std::string, size_t> with the workload string keys, else map<uint64_t, uint64_t>.
// The values and the index sit in 64-aligned memory before the clock starts, so no row measures the
// page cache or the page size (#301 does that). Prints the median ns per entry over the rounds and
// the peak RSS of one load in a forked child (max_rss.h), in bytes per entry.
#include "max_rss.h"

#include <ankerl/unordered_dense.h>
#include <bench/workloads.h>
#include <third-party/nanobench.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace ud = ankerl::unordered_dense;

#if defined(UDM_STR)
using key_t_ = std::string;
using val_t = std::size_t;
#else
using key_t_ = std::uint64_t;
using val_t = std::uint64_t;
#endif
using map_t = ud::map<key_t_, val_t>;
using view_t = map_t::view_type;
using clock_t_ = std::chrono::steady_clock;

auto keys(std::size_t n) -> std::vector<key_t_> {
    auto out = std::vector<key_t_>();
    out.reserve(n);
    auto rng = ankerl::nanobench::Rng(1);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(workloads::key_source<key_t_>::get(rng() >> 2U));
    }
    return out;
}

// 64-aligned copies of the two arrays, made once.
struct bytes {
    std::vector<map_t::value_type> values;
    map_t::index_block* blocks = nullptr;
    std::size_t num_blocks = 0;
    explicit bytes(map_t const& m)
        : values(m.values()) {
        num_blocks = m.index().size();
        blocks = static_cast<map_t::index_block*>(
            std::aligned_alloc(64, ((num_blocks * sizeof(map_t::index_block)) + 63) / 64 * 64));
        std::memcpy(static_cast<void*>(blocks), m.index().data(), num_blocks * sizeof(map_t::index_block));
    }
    ~bytes() {
        std::free(blocks);
    }
    bytes(bytes const&) = delete;
    auto operator=(bytes const&) -> bytes& = delete;
    [[nodiscard]] auto index() const -> map_t::index_view {
        return {blocks, num_blocks};
    }
};

// Whether the mode is handed the caller's values as a vector, which is then part of what it holds.
#if UDM_MODE_owning || UDM_MODE_owning_unchecked
constexpr bool takes_values = true;
#else
constexpr bool takes_values = false;
#endif
#if UDM_MODE_owning || UDM_MODE_view_checked
constexpr auto trust_level = ud::trust::checked;
#else
constexpr auto trust_level = ud::trust::unchecked;
#endif

// One load of the mode under test. A map it builds is moved into `keep` (O(1)), so that the caller
// destroys it after the clock stops: for strings the destructor is a free per string, which is not
// what a load costs.
template <typename Done>
void one([[maybe_unused]] std::vector<key_t_> const& ks,
         [[maybe_unused]] bytes const& b,
         [[maybe_unused]] map_t const& owning,
         [[maybe_unused]] std::vector<map_t::value_type>&& vals,
         [[maybe_unused]] std::optional<map_t>& keep,
         Done done) {
#if UDM_MODE_build
    auto m = map_t();
    m.reserve(ks.size());
    for (std::size_t i = 0; i < ks.size(); ++i) {
        m.try_emplace(ks[i], i);
    }
    done(m.size());
    keep.emplace(std::move(m));
#elif UDM_MODE_owning || UDM_MODE_owning_unchecked
    auto m = map_t(std::move(vals), b.index(), trust_level);
    done(m.size());
    keep.emplace(std::move(m));
#elif UDM_MODE_view_checked || UDM_MODE_view_unchecked
    auto v = view_t({b.values.data(), b.values.size()}, b.index(), trust_level);
    done(v.size());
#elif UDM_MODE_verify_spot
    done(owning.verify(ud::verify_level::spot) ? 1U : 0U);
#elif UDM_MODE_verify_full
    done(owning.verify(ud::verify_level::full) ? 1U : 0U);
#else
#    error "set -DUDM_MODE_<mode>=1"
#endif
}

} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    if (argc < 2) {
        std::printf("usage: %s <n> [rounds]\n", argv[0]);
        return 1;
    }
    auto const n = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
    auto const rounds = argc > 2 ? static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10)) : std::size_t{7};

    auto const ks = keys(n);
    auto owning = map_t();
    for (std::size_t i = 0; i < ks.size(); ++i) {
        owning.try_emplace(ks[i], i);
    }
    auto const b = bytes(owning);
    if (!owning.verify(ud::verify_level::spot)) {
        std::fprintf(stderr, "the source map does not verify\n");
        return 2;
    }

    // Peak resident memory of one load, the values copy included, as bytes per entry. First: after
    // the timed rounds the freed memory is resident in the arena and the child reads near zero.
    auto const rss = max_rss::of([&] {
                         auto vals = takes_values ? b.values : std::vector<map_t::value_type>();
                         auto keep = std::optional<map_t>();
                         one(ks, b, owning, std::move(vals), keep, [](std::size_t x) {
                             ankerl::nanobench::doNotOptimizeAway(x);
                         });
                     }) /
                     static_cast<double>(n);
    auto sink = std::size_t{0};
    auto done = [&](std::size_t x) {
        sink += x;
    };
    auto times = std::vector<double>();
    for (std::size_t r = 0; r < rounds + 1; ++r) {
        auto vals = takes_values ? b.values : std::vector<map_t::value_type>(); // outside the clock
        auto keep = std::optional<map_t>();
        auto const t0 = clock_t_::now();
        one(ks, b, owning, std::move(vals), keep, done);
        auto const t1 = clock_t_::now();
        keep.reset(); // outside the clock
        if (r != 0) { // the first round pays the first touch
            times.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(n));
        }
    }
    std::sort(times.begin(), times.end());
    ankerl::nanobench::doNotOptimizeAway(sink);

    std::printf("%zu ns/entry %.4f min %.4f max %.4f rss_B/entry %.1f\n",
                n,
                times[times.size() / 2],
                times.front(),
                times.back(),
                rss);
    return 0;
}
