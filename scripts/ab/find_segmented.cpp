// Instructions and cycles per find() and per contains(), map against segmented_map, counted
// in-process around the loop only (#312).
//
// One map, one operation, one mode and one size per binary, so each is the translation unit a
// caller compiles. Picked with -D:
//
//   SEGMENTED            segmented_map (else map)
//   HIT / MISS           every key present / every key absent, random order
//   CONTAINS             contains(k) (else find(k), then the mapped value if found)
//   STRING_KEY           std::string 8-40 bytes (else std::uint64_t through a bijection)
//   SIZE=n               entries (default 50000)
//
//   clang++ -O3 -DNDEBUG -std=c++17 -I<include dir> -DSEGMENTED -DHIT scripts/ab/find_segmented.cpp
//   ./a.out              # prints: map op mode key size instr/op cycles/op
//
// scripts/ab/find_segmented.sh builds a revision's header and the working tree's, both compilers.
#include <ankerl/unordered_dense.h>

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <string>
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

auto read_counter(int fd) -> std::uint64_t {
    std::uint64_t v = 0;
    if (read(fd, &v, sizeof(v)) != sizeof(v)) {
        return 0;
    }
    return v;
}

#ifdef STRING_KEY
using bench_key = std::string;
constexpr char const* key_name = "string";
auto make_key(rng& r) -> bench_key {
    auto len = 8 + r() % 33;
    auto s = std::string(len, '\0');
    for (auto& c : s) {
        c = static_cast<char>('a' + r() % 26);
    }
    return s;
}
#else
using bench_key = std::uint64_t;
constexpr char const* key_name = "uint64";
auto make_key(rng& r) -> bench_key {
    return r() * UINT64_C(0x9E3779B97F4A7C15);
}
#endif

volatile std::uint64_t sink = 0;

#ifdef SEGMENTED
using map_t = ankerl::unordered_dense::segmented_map<bench_key, std::uint64_t>;
constexpr char const* map_name = "segmented";
#else
using map_t = ankerl::unordered_dense::map<bench_key, std::uint64_t>;
constexpr char const* map_name = "map";
#endif

#ifndef SIZE
#    define SIZE 50000
#endif

} // namespace

int main() {
    constexpr std::size_t size = SIZE;
    constexpr std::size_t ops = std::size_t{1} << 23U;
    auto r = rng{};
    auto map = map_t{};
    auto keys = std::vector<bench_key>{};
    while (map.size() < size) {
        auto k = make_key(r);
        if (map.try_emplace(k, map.size()).second) {
            keys.push_back(std::move(k));
        }
    }
#if defined(HIT)
    constexpr char const* mode = "hit";
#elif defined(MISS)
    constexpr char const* mode = "miss";
    // Fresh keys, absent from the table: replace every key with one the table does not hold.
    for (auto& k : keys) {
        do {
            k = make_key(r);
        } while (map.contains(k));
    }
#else
#    error "define HIT or MISS"
#endif
    auto order = std::vector<std::uint32_t>(std::size_t{1} << 16U);
    for (auto& o : order) {
        o = static_cast<std::uint32_t>(r() % size);
    }

    int const fd_ins = open_counter(PERF_COUNT_HW_INSTRUCTIONS);
    int const fd_cyc = open_counter(PERF_COUNT_HW_CPU_CYCLES);
    if (fd_ins < 0 || fd_cyc < 0) {
        std::perror("perf_event_open");
        return 1;
    }
    ioctl(fd_ins, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(fd_cyc, PERF_EVENT_IOC_ENABLE, 0);
    for (std::size_t i = 0; i < ops; ++i) {
        auto const& k = keys[order[i & (order.size() - 1)]];
#ifdef CONTAINS
        sink = sink + static_cast<std::uint64_t>(map.contains(k));
#else
        auto it = map.find(k);
        sink = sink + (it != map.end() ? it->second : 1U);
#endif
    }
    ioctl(fd_ins, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(fd_cyc, PERF_EVENT_IOC_DISABLE, 0);
#ifdef CONTAINS
    constexpr char const* op = "contains";
#else
    constexpr char const* op = "find";
#endif
    auto const ins = static_cast<double>(read_counter(fd_ins));
    auto const cyc = static_cast<double>(read_counter(fd_cyc));
    std::printf("%s %s %s %s %zu %.2f %.2f\n", map_name, op, mode, key_name, size, ins / static_cast<double>(ops),
                cyc / static_cast<double>(ops));
    return 0;
}
