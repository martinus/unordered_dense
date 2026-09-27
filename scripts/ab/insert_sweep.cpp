// The size axis for operator[] and try_emplace (#310): one map per binary, map<uint64_t, uint64_t>,
// keys f(i) through a bijection so no key array is read in the timed loop.
//   -DHIT       ++m[f(i)] on present keys, i random in [0, N)
//   -DBUILD     ++m[f(i)] for i in [0, N) from empty, growing; repeated
//   -DCHURN     try_emplace(f(i)), erase it if it was there, i random in [0, 2N): size stays about N
//   -DNOINL     the operation behind a noinline function
//   -DREADONLY  HIT reads m[f(i)] instead of incrementing it
//   -DPOW2      HIT picks i with a mask instead of % N (N must be a power of two)
// ./insert_sweep N  prints: N instr/op cycles/op
//
// HIT and CHURN pick their key with `r() % n` and store into what they found, which is the trap in
// scripts/ab/spill_trap.cpp: wherever the compiler spills this loop's variables, those rows measure
// that and not the map (notes, "stored and reloaded"). -DREADONLY or -DPOW2 takes it out.
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
namespace {
struct rng {
    std::uint64_t s = 0x243f6a8885a308d3ULL;
    auto operator()() -> std::uint64_t {
        s ^= s << 13U;
        s ^= s >> 7U;
        s ^= s << 17U;
        return s;
    }
};
auto f(std::uint64_t i) -> std::uint64_t {
    i *= 0x9E3779B97F4A7C15ULL;
    return i ^ (i >> 29U);
}
auto open_counter(std::uint64_t config) -> int {
    perf_event_attr a{};
    a.size = sizeof(a);
    a.type = PERF_TYPE_HARDWARE;
    a.config = config;
    a.disabled = 1;
    a.exclude_kernel = 1;
    a.exclude_hv = 1;
    return static_cast<int>(syscall(SYS_perf_event_open, &a, 0, -1, -1, 0));
}
auto rd(int fd) -> double {
    std::uint64_t v = 0;
    return read(fd, &v, sizeof(v)) == sizeof(v) ? static_cast<double>(v) : 0.0;
}
using map_t = ankerl::unordered_dense::map<std::uint64_t, std::uint64_t>;
#ifdef NOINL
#    define OPATTR __attribute__((noinline))
#else
#    define OPATTR inline
#endif
#ifdef READONLY
[[maybe_unused]] OPATTR auto bump(map_t& m, std::uint64_t k) -> std::uint64_t {
    return m[k];
}
#else
[[maybe_unused]] OPATTR auto bump(map_t& m, std::uint64_t k) -> std::uint64_t {
    return ++m[k];
}
#endif
[[maybe_unused]] OPATTR auto churn_one(map_t& m, std::uint64_t k, std::uint64_t v) -> std::uint64_t {
    auto p = m.try_emplace(k, v);
    if (!p.second) {
        m.erase(p.first);
        return 0;
    }
    return 1;
}
volatile std::uint64_t sink = 0;
} // namespace
int main([[maybe_unused]] int argc, char** argv) {
    std::uint64_t const n = std::strtoull(argv[1], nullptr, 10);
    constexpr std::uint64_t min_ops = std::uint64_t{1} << 24U;
    [[maybe_unused]] auto r = rng{};
    int fi = open_counter(PERF_COUNT_HW_INSTRUCTIONS), fc = open_counter(PERF_COUNT_HW_CPU_CYCLES);
    double ins = 0, cyc = 0, ops = 0;
    std::uint64_t acc = 0;
#if defined(BUILD)
    std::uint64_t const rounds = n >= min_ops ? 2 : min_ops / n + 1;
    for (std::uint64_t round = 0; round < rounds + 1; ++round) {
        auto m = map_t();
        ioctl(fi, PERF_EVENT_IOC_RESET, 0);
        ioctl(fc, PERF_EVENT_IOC_RESET, 0);
        ioctl(fi, PERF_EVENT_IOC_ENABLE, 0);
        ioctl(fc, PERF_EVENT_IOC_ENABLE, 0);
        for (std::uint64_t i = 0; i < n; ++i)
            acc += bump(m, f(i + round * n));
        ioctl(fi, PERF_EVENT_IOC_DISABLE, 0);
        ioctl(fc, PERF_EVENT_IOC_DISABLE, 0);
        if (round > 0) {
            ins += rd(fi);
            cyc += rd(fc);
            ops += static_cast<double>(n);
        }
    }
#else
    auto m = map_t();
#    if defined(HIT)
    for (std::uint64_t i = 0; i < n; ++i)
        m.try_emplace(f(i), i);
#    else
    for (std::uint64_t i = 0; i < 2 * n; i += 2)
        m.try_emplace(f(i), i);
#    endif
    std::uint64_t const total = min_ops > 4 * n ? min_ops : 4 * n;
    for (int pass = 0; pass < 2; ++pass) {
        ioctl(fi, PERF_EVENT_IOC_RESET, 0);
        ioctl(fc, PERF_EVENT_IOC_RESET, 0);
        ioctl(fi, PERF_EVENT_IOC_ENABLE, 0);
        ioctl(fc, PERF_EVENT_IOC_ENABLE, 0);
        for (std::uint64_t j = 0; j < total; ++j) {
#    if defined(HIT)
#        ifdef POW2
            acc += bump(m, f(r() & (n - 1)));
#        else
            acc += bump(m, f(r() % n));
#        endif
#    else
            acc += churn_one(m, f(r() % (2 * n)), j);
#    endif
        }
        ioctl(fi, PERF_EVENT_IOC_DISABLE, 0);
        ioctl(fc, PERF_EVENT_IOC_DISABLE, 0);
        if (pass > 0) {
            ins += rd(fi);
            cyc += rd(fc);
            ops += static_cast<double>(total);
        }
    }
#endif
    sink = acc;
    std::printf("%llu %.2f %.2f\n", static_cast<unsigned long long>(n), ins / ops, cyc / ops);
}
