// The README's two benchmark graphs: every map a caller might reach for, in the configuration a
// caller gets by typing its type name, on the five things the README claims something about.
//
//   bench_readme <build|find|churn|iterate|memory> <base> [rounds]
//
// One map per binary -- `-DUDM_ONE_MAP=<index into maps_for<Key>::type>`, `-DUDM_ONE_STR` for the
// string key -- because a binary holding a dozen maps has a code layout that moves by more than the
// differences being drawn, and because a map that shares a process with eleven rivals shares their
// allocator state too. bench_readme.sh builds the set and collects the numbers.
//
// Every map is handed its *own* hash here (maps.h's UDM_DEFAULT_HASH), which is the one thing this
// harness does differently from maps.cpp next door. maps.cpp asks which index is faster and gives
// them all the same hash so that the index is what differs; a README graph is about what a caller
// gets, and what a caller gets includes the hash. For a string key the two questions have different
// answers, and both are worth having.
//
// Sizes are an octave: five points from n to just under 2n, geometric mean. A table's load factor
// sweeps a sawtooth between doublings and two maps double at different sizes, so a ratio taken at
// one size is a ratio between two arbitrary points of two different cycles -- measured at up to 26%
// for a cross-family pair (notes/index-design.md, "Fifty points draw the load-factor sawtooth").
#include "maps.h"

#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#ifndef UDM_ONE_MAP
#    define UDM_ONE_MAP 0
#endif

#if defined(UDM_COUNT_ALLOC)
// Only the `memory` binary is built with this. Interposing malloc is not free: measured on the
// integer build at a million entries, it costs std::unordered_map 4.4% and this map 0.0%, because
// the cost is per allocation and a dense map makes a handful where a node map makes a million. A
// bias that lands on one family and not the other is exactly the shape of error a chart like this
// must not have, so the four timed workloads run from a binary that has none of this compiled in.
// (An earlier version kept a sixteen byte header of its own on every block instead of reading
// glibc's, and that one cost std::unordered_map 29%.)
namespace {

// Live and peak bytes, counted at the two places the maps in this chart actually ask for memory:
// `malloc` (emilib calls it directly, and every `operator new` in libstdc++ goes through it) and
// `mmap` (the huge page allocator maps its own blocks and no heap counter can see them). Counting
// only `operator new` reported emilib at 0.00 bytes per entry and the huge-page segmented map at
// 0.35x the plain one, which is what a counter that cannot see an allocator looks like.
//
// Peak rather than steady, because peak is where a caller's ceiling is: a map that doubles holds
// the old array and the new one at the same moment.
std::size_t g_live = 0;
std::size_t g_peak = 0;
bool g_counting = false;

// What a block really costs, rather than what was asked for: glibc serves a request out of a chunk
// rounded to sixteen bytes with an eight byte header, so a node map asking a million times for
// twenty-four bytes is charged thirty-two each time. Counting the request instead reported every
// node map as *cheaper* per entry than this one. `malloc_usable_size` plus the header is the same
// policy scripts/ab/alloc_timeline.cpp already measures by, and it is the reason there is no header
// of our own here: the size is recoverable from the pointer, so free() needs no bookkeeping and
// blocks from aligned_alloc -- which an over-aligned `operator new` uses and this never sees the
// allocation of -- pass through correctly.
auto charged(void* p) -> std::size_t {
    return p == nullptr ? 0 : malloc_usable_size(p) + sizeof(std::size_t);
}

// The g_counting test lives here rather than in each of the six interposers below.
void counted(void* p) {
    if (g_counting) {
        g_live += charged(p);
        g_peak = g_live > g_peak ? g_live : g_peak;
    }
}

void counted_bytes(std::size_t n) {
    if (g_counting) {
        g_live += n;
        g_peak = g_live > g_peak ? g_live : g_peak;
    }
}

void released(std::size_t n) {
    if (g_counting) {
        g_live -= n < g_live ? n : g_live;
    }
}

} // namespace

extern "C" {

// glibc's own, which is what keeps the interposers below from recursing into themselves.
void* __libc_malloc(std::size_t n);
void* __libc_calloc(std::size_t count, std::size_t size);
void* __libc_realloc(void* p, std::size_t n);
void __libc_free(void* p);

void* malloc(std::size_t n) {
    auto* p = __libc_malloc(n);
    counted(p);
    return p;
}

void* calloc(std::size_t count, std::size_t size) {
    auto* p = __libc_calloc(count, size);
    counted(p);
    return p;
}

void free(void* p) noexcept {
    released(charged(p));
    __libc_free(p);
}

void* realloc(void* p, std::size_t n) {
    released(charged(p));
    auto* fresh = __libc_realloc(p, n);
    counted(fresh);
    return fresh;
}

// The huge page allocator's blocks, which never touch the heap at all.
void* mmap(void* addr, std::size_t len, int prot, int flags, int fd, off_t offset) {
    // dlsym rather than a __ alias: glibc exports mmap64 under several names and the alias that
    // exists is not the same on every build.
    static auto* real = reinterpret_cast<void* (*)(void*, std::size_t, int, int, int, off_t)>(dlsym(RTLD_NEXT, "mmap"));
    auto* p = real(addr, len, prot, flags, fd, offset);
    if (p != MAP_FAILED) { // NOLINT(cppcoreguidelines-pro-type-cstyle-cast,performance-no-int-to-ptr)
        counted_bytes(len);
    }
    return p;
}

int munmap(void* addr, std::size_t len) {
    released(len);
    static auto* real = reinterpret_cast<int (*)(void*, std::size_t)>(dlsym(RTLD_NEXT, "munmap"));
    return real(addr, len);
}

} // extern "C"
#endif // UDM_COUNT_ALLOC

namespace {
using namespace udm_maps;

#if defined(UDM_ONE_STR)
using one_key = std::string;
#else
using one_key = std::uint64_t;
#endif
using one_val = std::size_t;
using map_t = std::tuple_element_t<UDM_ONE_MAP, typename maps_for<one_key, one_val>::type>;

using clock_t_ = std::chrono::steady_clock;

// The other way to ask what a map costs: peak resident set, which is what the kernel actually
// handed the process. It differs from the counted bytes in both directions -- it includes the
// allocator's slack, its arena rounding and whole huge pages, and it excludes anything asked for
// and never touched -- so the two numbers answering differently is informative rather than a bug.
auto status_kb(char const* field) -> std::size_t {
    auto* f = std::fopen("/proc/self/status", "r");
    if (f == nullptr) {
        return 0;
    }
    char line[256];
    auto const n = std::strlen(field);
    auto out = std::size_t{0};
    while (std::fgets(line, sizeof(line), f) != nullptr) {
        if (std::strncmp(line, field, n) == 0) {
            out = std::strtoull(line + n + 1, nullptr, 10);
            break;
        }
    }
    std::fclose(f);
    return out;
}

// Linux resets the high-water mark to the current RSS when 5 is written here (CLEAR_REFS_MM_HIWATER_RSS).
void reset_peak_rss() {
    auto* f = std::fopen("/proc/self/clear_refs", "w");
    if (f != nullptr) {
        std::fputs("5\n", f);
        std::fclose(f);
    }
}

// One octave, log-spaced: the i-th of five points is base * 2^(i/5), so the set spans exactly one
// doubling whatever the base is.
auto octave_size(std::size_t base, std::size_t i) -> std::size_t {
    return static_cast<std::size_t>(static_cast<double>(base) * std::pow(2.0, static_cast<double>(i) / 5.0));
}

// How much work a timed region does, whatever the table size: a cell that runs for a millisecond
// measures the machine's mood, and the ratios this chart draws are 3-20% apart. Ten million
// operations is 1.4 to 6.2 seconds per cell depending on the map and the workload, which is where
// repeated runs of the same cell agree to under a percent.
constexpr std::size_t target_ops = 10'000'000;

// ns per operation at one size, or bytes per entry for `memory`.
//
// One untimed round first, then the timed one. Without the warm-up the first size of an octave
// reads up to 2.3x the third, because it is the one paying for the process's first touch of every
// page it asks for. There is no round count here on purpose: the repeats that get medianed are the
// shell's, which interleaves them across every map rather than repeating one cell back to back, so
// that a machine drifting across the run drifts across all of them.
auto one_size(std::string const& work, std::size_t n) -> double {
    auto p = pools<one_key>(n);

    auto once = [&]() -> double {
        if (work == "memory") {
#if !defined(UDM_COUNT_ALLOC)
            std::fprintf(stderr, "this binary was built without UDM_COUNT_ALLOC and cannot count bytes\n");
            std::exit(2);
#else
            // The keys are built before the counter goes true, so the pools are not charged to the
            // map. What is counted is the peak of the build, which includes the doubling: the
            // moment the old array and the new one are both alive is where a caller's ceiling is.
            auto m = map_t();
            g_live = 0;
            g_peak = 0;
            g_counting = true;
            fill(m, p);
            auto const peak = g_peak;
            g_counting = false;
            return static_cast<double>(peak) / static_cast<double>(n);
#endif
        }
        if (work == "rss") {
            // One fill per *process*, in a fork. A peak RSS taken after some earlier map has been
            // built and freed reads whatever the allocator decided to keep rather than what this
            // map costs: glibc does not return a grown arena, so the second fill reuses resident
            // pages and the number comes out below the bytes the map actually asked for. The pools
            // are built before the fork and only read, so they are resident in the baseline and
            // never charged, and no page of theirs is copied.
            auto fd = std::array<int, 2>{};
            if (pipe(fd.data()) != 0) {
                return 0.0;
            }
            auto const pid = fork();
            if (pid == 0) {
                close(fd[0]);
                auto m = map_t();
                auto const before = status_kb("VmRSS");
                reset_peak_rss();
                fill(m, p);
                auto const peak = status_kb("VmHWM");
                auto const v = static_cast<double>((peak - before) * 1024) / static_cast<double>(n);
                auto const ignored = write(fd[1], &v, sizeof(v));
                static_cast<void>(ignored);
                _exit(0);
            }
            close(fd[1]);
            auto v = 0.0;
            auto const got = read(fd[0], &v, sizeof(v));
            close(fd[0]);
            waitpid(pid, nullptr, 0);
            return got == sizeof(v) ? v : 0.0;
        }
        if (work == "buildfree") {
            // Build *and* tear down: the same loop as `build` with the destructor left inside the
            // clock, so that the two panels together say how much of a map's lifetime cost is the
            // giving back. A node map hands a million blocks to free(); a dense map frees two.
            auto const builds = target_ops / n + 1;
            auto acc = std::size_t{0};
            auto const t0 = clock_t_::now();
            for (std::size_t i = 0; i < builds; ++i) {
                auto m = map_t();
                fill(m, p);
                acc += m.size();
            }
            auto const t1 = clock_t_::now();
            ankerl::nanobench::doNotOptimizeAway(acc);
            return static_cast<double>((t1 - t0).count()) / static_cast<double>(builds * n);
        }
        if (work == "build") {
            // The only workload whose subject is created inside the timed region, because creating
            // it is what it measures. Its *destruction* is outside, which maps.h's build() cannot
            // do because it returns after the map has gone: giving back a million nodes costs
            // `std::unordered_map` about a third of this loop again and a dense map nothing, so
            // timing it would put a teardown difference on a panel named for inserts.
            auto const builds = target_ops / n + 1;
            auto acc = std::size_t{0};
            auto total = clock_t_::duration::zero();
            for (std::size_t i = 0; i < builds; ++i) {
                auto const t0 = clock_t_::now();
                auto m = map_t();
                fill(m, p);
                auto const t1 = clock_t_::now();
                total += t1 - t0;
                acc += m.size();
            }
            ankerl::nanobench::doNotOptimizeAway(acc);
            return static_cast<double>(total.count()) / static_cast<double>(builds * n);
        }

        // Everything else measures a table that is already there, so the fill is outside the timed
        // region: what is timed is the operation the panel is named after.
        auto m = map_t();
        fill(m, p);
        auto ops = target_ops;
        auto const t0 = clock_t_::now();
        if (work == "find") {
            auto st = lookup_state();
            ankerl::nanobench::doNotOptimizeAway(lookups(m, p, st, asking::half, ops));
        } else if (work == "churn") {
            auto rng = ankerl::nanobench::Rng(7);
            auto tick = std::size_t{0};
            churn(m, p.present, p.spare, rng, ops, tick);
            ankerl::nanobench::doNotOptimizeAway(m.size());
        } else {
            // One operation is one element visited, so the round count is what makes the element
            // count the same for every size.
            auto const sweeps = target_ops / n + 1;
            auto acc = std::size_t{0};
            for (std::size_t i = 0; i < sweeps; ++i) {
                acc += m.sum();
            }
            ankerl::nanobench::doNotOptimizeAway(acc);
            ops = sweeps * n;
        }
        auto const t1 = clock_t_::now();
        return static_cast<double>((t1 - t0).count()) / static_cast<double>(ops);
    };

    if (work == "rss") {
        return once(); // each call forks; there is no process state to warm
    }
    once(); // discarded
    return once();
}

} // namespace

auto main(int argc, char** argv) -> int {
    workloads::tame_allocator();
    if (argc < 3) {
        std::printf("usage: %s <build|buildfree|find|churn|iterate|memory|rss> <base>\n", argv[0]);
        return 1;
    }
    auto const work = std::string(argv[1]);
    auto const base = static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10));
    // One list, checked before a million-entry table gets built rather than after.
    static constexpr auto known =
        std::array<char const*, 7>{"build", "buildfree", "find", "churn", "iterate", "memory", "rss"};
    if (std::none_of(known.begin(), known.end(), [&](char const* k) {
            return work == k;
        })) {
        std::fprintf(stderr, "unknown workload %s\n", work.c_str());
        return 2;
    }

    // The geometric mean over the octave, which is the summary a ratio between two maps can be
    // taken from; the per-size numbers are printed beside it so a wild one is visible.
    auto acc = 0.0;
    std::printf("%s %s", map_t::name, work.c_str());
    for (std::size_t i = 0; i < 5; ++i) {
        auto const v = one_size(work, octave_size(base, i));
        acc += std::log(v);
        std::printf(" %.4f", v);
    }
    std::printf(" geomean %.4f\n", std::exp(acc / 5.0));
    return 0;
}
