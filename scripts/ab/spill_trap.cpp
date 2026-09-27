// No map: a spilled loop variable, a 64-bit division and a store with a late address, together,
// make the iterations of a lookup loop stop overlapping their cache misses (#310). Two arrays of 16M
// entries; the loop computes a key, loads idx[key] (a miss), loads val[idx[key]] (a miss).
//   -DDIV                key = r % n, else r & (n - 1)
//   -DSTORE              ++ the found element: a store whose address comes from a miss
//   -DSTORE_FIXED        instead, ++ one fixed element: a store whose address is known at once
//   -DSPILL              the generator state and the divisor go through memory every iteration,
//                        stored and reloaded, the way a spilled loop variable does
//   -DSPILL_DIVISOR_ONLY only the divisor is reloaded from memory, never stored
// Ryzen 9 7950X, cycles per iteration: 48-76 for any one or two of SPILL, DIV, STORE; 381 for all
// three; 65 with STORE_FIXED instead of STORE. The same under clang 22 and gcc 16.
//
//   clang++ -O2 -std=c++17 -DSPILL -DDIV -DSTORE scripts/ab/spill_trap.cpp -o t && taskset -c 2 ./t
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
namespace {
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
volatile std::uint64_t sink = 0;
} // namespace
int main(int argc, char** argv) {
    std::uint64_t const n = std::uint64_t{1} << 24U; // 16M entries, 128 MB each array: past the cache
    std::uint64_t divisor = argc > 5 ? 3 : n;        // opaque to the compiler, equal to n
    auto idx = std::vector<std::uint32_t>(n);
    auto val = std::vector<std::uint64_t>(n);
    std::uint64_t s = 0x243f6a8885a308d3ULL;
    for (auto& x : idx) {
        s ^= s << 13U;
        s ^= s >> 7U;
        s ^= s << 17U;
        x = static_cast<std::uint32_t>(s & (n - 1));
    }
    constexpr std::uint64_t ops = std::uint64_t{1} << 25U;
    int fi = open_counter(PERF_COUNT_HW_INSTRUCTIONS), fc = open_counter(PERF_COUNT_HW_CPU_CYCLES);
    ioctl(fi, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(fc, PERF_EVENT_IOC_ENABLE, 0);
    std::uint64_t acc = 0;
    for (std::uint64_t j = 0; j < ops; ++j) {
#ifdef SPILL
        asm volatile("" : "+m"(s));
        asm volatile("" : "+m"(divisor));
#endif
#ifdef SPILL_DIVISOR_ONLY
        asm volatile("" : : "m"(divisor));
        std::uint64_t d2;
        asm volatile("mov %1, %0" : "=r"(d2) : "m"(divisor));
#endif
        s ^= s << 13U;
        s ^= s >> 7U;
        s ^= s << 17U;
#if defined(SPILL_DIVISOR_ONLY)
        auto const k = s % d2;
#elif defined(DIV)
        auto const k = s % divisor;
#else
        auto const k = s & (n - 1);
#endif
        auto const i = idx[k]; // miss 1
        auto& v = val[i];      // miss 2, address from miss 1
#if defined(STORE_FIXED)
        acc += v;
        ++val[0];
#elif defined(STORE)
        acc += ++v;
#else
        acc += v;
#endif
    }
    ioctl(fi, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(fc, PERF_EVENT_IOC_DISABLE, 0);
    sink = acc;
    std::printf("instr/op %.2f cycles/op %.2f\n", rd(fi) / ops, rd(fc) / ops);
    (void)argv;
}
