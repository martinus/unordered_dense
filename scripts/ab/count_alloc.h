#pragma once
// Bytes a map asks for, counted at the two places the maps in this repository actually ask: the
// companion to max_rss.h, which counts what the kernel backed. The two answer different questions
// and disagree by more than either of them varies -- a doubling flat map is about 2.5x its counted
// bytes in resident pages, where a dense one is about 1.7x, because every superseded slot array
// stays resident in glibc's arena. Quote them together or say which one is meant.
//
// **This must not be linked into a binary that measures time.** Interposing malloc is not free:
// measured on the integer build at a million entries it costs std::unordered_map 4.4% and this map
// 0.0%, because the cost is per allocation and a dense map makes a handful where a node map makes a
// million. A bias that lands on one family and not the other is exactly the shape of error a
// comparison must not have. Build the counting binary separately, under -DUDM_COUNT_ALLOC.
//
// Everything below is compiled away without that macro, so including this header costs nothing.
#include <cstddef>

#include <sys/wait.h>
#include <unistd.h>

#if defined(UDM_COUNT_ALLOC)
#    include <dlfcn.h>
#    include <malloc.h>
#    include <sys/mman.h>
#endif

#if defined(UDM_COUNT_ALLOC)
// Only the `memory` binary is built with this. Interposing malloc is not free: measured on the
// integer build at a million entries, it costs std::unordered_map 4.4% and this map 0.0%, because
// the cost is per allocation and a dense map makes a handful where a node map makes a million. A
// bias that lands on one family and not the other is exactly the shape of error a chart like this
// must not have, so the four timed workloads run from a binary that has none of this compiled in.
// (An earlier version kept a sixteen byte header of its own on every block instead of reading
// glibc's, and that one cost std::unordered_map 29%.)
namespace count_alloc {

// Live and peak bytes, counted at the two places the maps in this chart actually ask for memory:
// `malloc` (emilib calls it directly, and every `operator new` in libstdc++ goes through it) and
// `mmap` (the huge page allocator maps its own blocks and no heap counter can see them). Counting
// only `operator new` reported emilib at 0.00 bytes per entry and the huge-page segmented map at
// 0.35x the plain one, which is what a counter that cannot see an allocator looks like.
//
// Peak rather than steady, because peak is where a caller's ceiling is: a map that doubles holds
// the old array and the new one at the same moment.
inline std::size_t g_live = 0;
inline std::size_t g_peak = 0;
inline bool g_counting = false;
inline std::size_t g_marked = 0;

// What a block really costs, rather than what was asked for: glibc serves a request out of a chunk
// rounded to sixteen bytes with an eight byte header, so a node map asking a million times for
// twenty-four bytes is charged thirty-two each time. Counting the request instead reported every
// node map as *cheaper* per entry than this one. `malloc_usable_size` plus the header is the same
// policy scripts/ab/alloc_timeline.cpp already measures by, and it is the reason there is no header
// of our own here: the size is recoverable from the pointer, so free() needs no bookkeeping and
// blocks from aligned_alloc -- which an over-aligned `operator new` uses and this never sees the
// allocation of -- pass through correctly.
inline auto charged(void* p) -> std::size_t {
    return p == nullptr ? 0 : malloc_usable_size(p) + sizeof(std::size_t);
}

// The g_counting test lives here rather than in each of the six interposers below.
inline void counted(void* p) {
    if (g_counting) {
        g_live += charged(p);
        g_peak = g_live > g_peak ? g_live : g_peak;
    }
}

inline void counted_bytes(std::size_t n) {
    if (g_counting) {
        g_live += n;
        g_peak = g_live > g_peak ? g_live : g_peak;
    }
}

inline void released(std::size_t n) {
    if (g_counting) {
        g_live -= n < g_live ? n : g_live;
    }
}

} // namespace count_alloc

using count_alloc::counted;
using count_alloc::counted_bytes;
using count_alloc::released;
using count_alloc::charged;

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

// Over-aligned blocks. `free` already charges these correctly, because malloc_usable_size works on
// them, but nothing was counting them on the way in: ihtab asks for its group array with
// std::aligned_alloc and so reported 8.3 bytes per entry against a true ~36, with its element
// array counted and its whole index invisible. An allocator a counter cannot see is the same
// failure as counting only `operator new` and missing emilib's direct malloc.
void* aligned_alloc(std::size_t alignment, std::size_t n) noexcept {
    static auto* real = reinterpret_cast<void* (*)(std::size_t, std::size_t)>(dlsym(RTLD_NEXT, "aligned_alloc"));
    auto* p = real(alignment, n);
    counted(p);
    return p;
}

int posix_memalign(void** out, std::size_t alignment, std::size_t n) noexcept {
    static auto* real =
        reinterpret_cast<int (*)(void**, std::size_t, std::size_t)>(dlsym(RTLD_NEXT, "posix_memalign"));
    auto const rc = real(out, alignment, n);
    if (rc == 0) {
        counted(*out);
    }
    return rc;
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

namespace count_alloc {

// Where the map was at its fullest, and what it had asked for by then. A caller marks the moment
// its subject is alive and before anything is torn down; `live` is the design number (what the
// container holds) and `peak` includes the transient where a doubling container owns the old array
// and the new one at once.
struct result {
    double live;
    double peak;
};

inline void mark() {
#if defined(UDM_COUNT_ALLOC)
    g_marked = g_live;
#endif
}

// Bytes `work` asks for, measured **in a forked child**.
//
// The fork is not about the arena being warm for the measurement -- requests are counted, not
// pages, so a warm arena cannot flatter this one. It is about what filling a map here does to
// everything measured *afterwards*: glibc does not hand a grown arena back, so a counted pass run
// in the caller's own process leaves hundreds of megabytes resident and freed, and the next
// max_rss::of child then serves its whole allocation out of that without faulting a page. Measured
// without the fork, every map after the first read 2 to 20 resident bytes per entry against a true
// 50 to 70, and emilib read 2.0.
template <typename F>
auto of(F&& work) -> result {
#if defined(UDM_COUNT_ALLOC)
    int fd[2];
    if (pipe(fd) != 0) {
        return {0.0, 0.0};
    }
    auto const pid = fork();
    if (pid == 0) {
        close(fd[0]);
        g_live = 0;
        g_peak = 0;
        g_marked = 0;
        g_counting = true;
        work();
        g_counting = false;
        double v[2] = {static_cast<double>(g_marked != 0 ? g_marked : g_peak), static_cast<double>(g_peak)};
        auto const ignored = write(fd[1], &v, sizeof(v));
        static_cast<void>(ignored);
        _exit(0);
    }
    close(fd[1]);
    double v[2] = {0.0, 0.0};
    auto const got = read(fd[0], &v, sizeof(v));
    close(fd[0]);
    waitpid(pid, nullptr, 0);
    return got == static_cast<ssize_t>(sizeof(v)) ? result{v[0], v[1]} : result{0.0, 0.0};
#else
    static_cast<void>(work);
    return {0.0, 0.0};
#endif
}

// Whether this binary can count at all, so a harness can say so rather than print zeros.
inline constexpr auto available() -> bool {
#if defined(UDM_COUNT_ALLOC)
    return true;
#else
    return false;
#endif
}

} // namespace count_alloc
