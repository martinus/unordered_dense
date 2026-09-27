// MySQL 9.7.2's set-operation map, replayed: segmented_map<8-byte string key, pointer> built from
// empty by emplace(key, mapped{nullptr}), 49% of the keys new. Instructions and cycles per emplace,
// the first of eight builds discarded. One map per binary; MySQL builds with -O2.
//
//   g++ -O2 -DNDEBUG -std=c++17 -Iinclude scripts/ab/mysqlish.cpp -o mysqlish && taskset -c 2 ./mysqlish
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <linux/perf_event.h>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
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
struct linked {
    char const* p;
};
using map_t = ankerl::unordered_dense::segmented_map<std::string_view, linked>;
__attribute__((noinline)) std::size_t run(map_t& m, std::vector<std::string_view> const& keys) {
    std::size_t fresh = 0;
    for (auto const& k : keys)
        fresh += m.emplace(k, linked{nullptr}).second ? 1U : 0U;
    return fresh;
}
} // namespace
int main() {
    constexpr std::size_t n = 1000000, distinct = 600000, rounds = 8;
    auto r = rng{};
    auto store = std::vector<std::uint64_t>(distinct);
    for (auto& v : store)
        v = r() * UINT64_C(0x9E3779B97F4A7C15);
    auto keys = std::vector<std::string_view>(n);
    for (auto& k : keys)
        k = std::string_view(reinterpret_cast<char const*>(&store[r() % distinct]), 8);
    int fi = open_counter(PERF_COUNT_HW_INSTRUCTIONS), fc = open_counter(PERF_COUNT_HW_CPU_CYCLES);
    std::size_t fresh = 0;
    double ins = 0, cyc = 0;
    for (std::size_t i = 0; i < rounds; ++i) {
        auto m = map_t();
        ioctl(fi, PERF_EVENT_IOC_RESET, 0);
        ioctl(fc, PERF_EVENT_IOC_RESET, 0);
        ioctl(fi, PERF_EVENT_IOC_ENABLE, 0);
        ioctl(fc, PERF_EVENT_IOC_ENABLE, 0);
        fresh = run(m, keys);
        ioctl(fi, PERF_EVENT_IOC_DISABLE, 0);
        ioctl(fc, PERF_EVENT_IOC_DISABLE, 0);
        if (i > 0) {
            ins += rd(fi);
            cyc += rd(fc);
        }
    }
    std::printf(
        "instr %.2f cycles %.2f fresh %.3f\n", ins / (rounds - 1) / n, cyc / (rounds - 1) / n, static_cast<double>(fresh) / n);
}
