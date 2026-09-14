#pragma once
// Peak resident set for one piece of work, which is the honest answer to "what does this map cost":
// what the kernel actually backed, including the allocator's slack and rounding and whole huge
// pages, and excluding anything asked for and never touched. Counting bytes requested cannot see
// either end of that -- a doubling flat map leaves every superseded array freed but resident in
// glibc's arena, worth about a third again per entry, and a huge page allocator's mapping is
// charged in full whether its pages are touched or not.
//
// Two things this has to do or the number is wrong:
//
//   * measure in a forked child. glibc does not hand a grown arena back, so a second fill in the
//     same process reuses resident pages and reads far below what the map demonstrably allocated.
//     The child also means the caller's own state -- key pools, earlier maps -- is in the baseline
//     and never charged, and nothing it does is copied because it only reads.
//   * reset VmHWM first. It is a high-water mark for the life of the process; writing 5 to
//     /proc/self/clear_refs (CLEAR_REFS_MM_HIWATER_RSS, Linux 4.0+) sets it back to the current RSS.
//
// Huge pages are counted correctly and were checked rather than assumed: a 1M-entry
// `huge_page::map` reports 28 MB of AnonHugePages in /proc/self/smaps_rollup and a VmHWM 2 MB above
// the same map on 4 KB pages, which is the rounding, resident, as it should be.
//
// One build is enough. The result is deterministic to the page: repeated rounds of the same cell
// come back byte-identical, so there is nothing for a median to do.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include <sys/wait.h>
#include <unistd.h>

namespace max_rss {

inline auto status_kb(char const* field, char const* file = "/proc/self/status") -> long {
    auto* f = std::fopen(file, "r");
    if (f == nullptr) {
        return 0;
    }
    char line[256];
    auto const n = std::strlen(field);
    auto out = 0L;
    while (std::fgets(line, sizeof(line), f) != nullptr) {
        if (std::strncmp(line, field, n) == 0) {
            out = std::strtol(line + n + 1, nullptr, 10);
            break;
        }
    }
    std::fclose(f);
    return out;
}

inline void reset_peak() {
    auto* f = std::fopen("/proc/self/clear_refs", "w");
    if (f != nullptr) {
        std::fputs("5\n", f);
        std::fclose(f);
    }
}

// Bytes of peak resident set that a piece of work drives, before the floor below is taken off.
template <typename F>
auto gross(F&& work) -> double {
    int fd[2];
    if (pipe(fd) != 0) {
        return 0.0;
    }
    auto const pid = fork();
    if (pid == 0) {
        close(fd[0]);
        auto const before = status_kb("VmRSS");
        reset_peak();
        work();
        auto const peak = status_kb("VmHWM");
        auto const v = static_cast<double>(peak - before) * 1024.0;
        auto const ignored = write(fd[1], &v, sizeof(v));
        static_cast<void>(ignored);
        _exit(0);
    }
    close(fd[1]);
    auto v = 0.0;
    auto const got = read(fd[0], &v, sizeof(v));
    close(fd[0]);
    waitpid(pid, nullptr, 0);
    return got == static_cast<ssize_t>(sizeof(v)) ? v : 0.0;
}

// Bytes of peak resident set that `work` is responsible for.
//
// A child that does nothing at all does not read zero: forking, writing /proc/self/clear_refs and
// reading /proc/self/status fault in about 128 KB of their own, and that floor is charged to
// whatever is being measured. It is a constant rather than drift -- five repetitions of a cell come
// back byte-identical, and growing the parent by 256 MB does not move it -- so the honest thing is
// to measure it next to every cell and take it off. Before this was done the panel read 200-500
// bytes per entry for a thousand-entry map holding 16 KB of data, and cells at that size disagreed
// by up to 1.22x between two runs of the same binary while cells at half a million agreed to 0.5%.
//
// What survives is a residual of about 20 KB, because the floor is mildly work-dependent, plus a
// few tens of KB of variation between processes. So this measures a map of a few megabytes well and
// a map of a few kilobytes not at all: quote it where the subject is large against the residual,
// which for the octaves this repository sweeps means 32000 entries and up, and do not report a
// thousand-entry column from it.
template <typename F>
auto of(F&& work) -> double {
    auto const loaded = gross(std::forward<F>(work));
    auto const floor_of_the_instrument = gross([] {});
    auto const v = loaded - floor_of_the_instrument;
    return v > 0.0 ? v : 0.0;
}

} // namespace max_rss
