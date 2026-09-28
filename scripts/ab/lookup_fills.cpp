// Why boost::unordered_flat_map finds faster than this map (#341): a bare lookup loop over a
// map<int64_t, 8 byte value> of TABLE_SIZE entries, 2^20 keys drawn beforehand, ROUNDS passes.
// Counters from perf_event_open around the loop. The fill sources need perf stat around the whole
// process (the loop dominates it with ROUNDS=8 and up):
//
//   clang++ -O3 -DNDEBUG -std=c++17 -Iinclude [-DBOOST_MAP] -DTABLE_SIZE=92160 scripts/ab/lookup_fills.cpp
//   taskset -c 2 perf stat -e ls_dmnd_fills_from_sys.local_l2,ls_dmnd_fills_from_sys.local_ccx,cycles:u ./a.out
#include <ankerl/unordered_dense.h>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdint>
#include <cstdio>
#include <linux/perf_event.h>
#include <random>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
struct bs {
    std::int32_t n;
    std::uint32_t s;
};
static int open_counter(std::uint32_t type, std::uint64_t config) {
    perf_event_attr a{};
    a.size = sizeof(a);
    a.type = type;
    a.config = config;
    a.disabled = 1;
    a.exclude_kernel = 1;
    a.exclude_hv = 1;
    return static_cast<int>(syscall(SYS_perf_event_open, &a, 0, -1, -1, 0));
}
constexpr std::uint64_t cache(std::uint64_t id, std::uint64_t op, std::uint64_t res) {
    return id | (op << 8) | (res << 16);
}
volatile std::uint64_t sink;
#ifndef ROUNDS
#    define ROUNDS 8
#endif
#ifndef TABLE_SIZE
#    define TABLE_SIZE 92160
#endif
int main() {
#ifdef BOOST_MAP
    boost::unordered_flat_map<std::int64_t, bs> m;
    char const* name = "boost";
#else
    ankerl::unordered_dense::map<std::int64_t, bs> m;
    char const* name = "ud";
#endif
    for (std::int64_t g = 0; g < TABLE_SIZE; ++g)
        m[g] = bs{static_cast<std::int32_t>(g % 72), static_cast<std::uint32_t>(g % 16)};
    std::mt19937_64 gen{1};
    std::vector<std::int64_t> keys(1 << 20);
    for (auto& k : keys)
        k = static_cast<std::int64_t>(gen() % TABLE_SIZE);
    int fds[4] = {open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES),
                  open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS),
                  open_counter(PERF_TYPE_HW_CACHE,
                               cache(PERF_COUNT_HW_CACHE_L1D, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS)),
                  open_counter(PERF_TYPE_HW_CACHE,
                               cache(PERF_COUNT_HW_CACHE_DTLB, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS))};
    for (int fd : fds)
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    std::uint64_t acc = 0;
    for (int r = 0; r < ROUNDS; ++r)
        for (auto k : keys) {
            auto it = m.find(k);
            acc += it != m.end() ? it->second.s : 1;
        }
    for (int fd : fds)
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    double v[4];
    for (int i = 0; i < 4; ++i) {
        std::uint64_t x = 0;
        if (read(fds[i], &x, 8) != 8)
            x = 0;
        v[i] = double(x) / (double(ROUNDS) * keys.size());
    }
    sink = acc;
    std::printf("%-6s N=%d  cyc %.2f  instr %.2f  L1D-miss %.3f  dTLB-miss %.4f  per find\n",
                name,
                TABLE_SIZE,
                v[0],
                v[1],
                v[2],
                v[3]);
}
