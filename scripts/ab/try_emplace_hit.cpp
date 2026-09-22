// Instructions and cycles per try_emplace, counted in-process around the loop only (#305).
//
// One map and one mode per binary, so the translation unit is the size a caller compiles and
// nothing else competes for the inliner's budget. Modes, picked with -D:
//
//   HIT   try_emplace on a key that is present, keys in random order
//   MISS  try_emplace of a fresh key into a table reserved for all of them
//
// and the key type with -DSTRING_KEY (std::string, 8-40 bytes) or not (std::uint64_t through a
// bijection). Counters come from perf_event_open, user space only, around the timed loop; the loop
// itself (an index load, a key load, a sink) is the same in every build, so differences between
// two headers are the map's.
//
//   clang++ -O3 -DNDEBUG -std=c++17 -I<include dir> -DHIT scripts/ab/try_emplace_hit.cpp -o hit
//   ./hit            # prints: mode key instr/op cycles/op
//
// scripts/ab/try_emplace_hit.sh builds both headers, both compilers, all four cells.
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

} // namespace

int main() {
    constexpr std::size_t size = 50000;
    constexpr std::size_t ops = std::size_t{1} << 22U;
    auto r = rng{};
    auto map = ankerl::unordered_dense::map<bench_key, std::uint64_t>{};

#if defined(HIT)
    constexpr char const* mode = "hit";
    auto keys = std::vector<bench_key>{};
    while (map.size() < size) {
        auto k = make_key(r);
        if (map.try_emplace(k, map.size()).second) {
            keys.push_back(std::move(k));
        }
    }
    auto order = std::vector<std::uint32_t>(std::size_t{1} << 16U);
    for (auto& o : order) {
        o = static_cast<std::uint32_t>(r() % size);
    }
#elif defined(MISS)
    constexpr char const* mode = "miss";
    // A fresh table of `size` entries per round, reserved: every key is new and nothing grows.
    constexpr std::size_t rounds = ops / size;
    auto keys = std::vector<bench_key>{};
    {
        auto seen = ankerl::unordered_dense::set<bench_key>{};
        while (keys.size() < size * 8) {
            auto k = make_key(r);
            if (seen.insert(k).second) {
                keys.push_back(std::move(k));
            }
        }
    }
    auto maps = std::vector<ankerl::unordered_dense::map<bench_key, std::uint64_t>>(rounds);
    for (auto& m : maps) {
        m.reserve(size);
    }
#else
#    error "define HIT or MISS"
#endif

    int const fd_ins = open_counter(PERF_COUNT_HW_INSTRUCTIONS);
    int const fd_cyc = open_counter(PERF_COUNT_HW_CPU_CYCLES);
    if (fd_ins < 0 || fd_cyc < 0) {
        std::perror("perf_event_open");
        return 1;
    }
    ioctl(fd_ins, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(fd_cyc, PERF_EVENT_IOC_ENABLE, 0);

#if defined(HIT)
    for (std::size_t i = 0; i < ops; ++i) {
        auto const& k = keys[order[i & (order.size() - 1)]];
        sink = sink + map.try_emplace(k, 0).first->second;
    }
    constexpr std::size_t done = ops;
#else
    std::size_t done = 0;
    for (std::size_t round = 0; round < rounds; ++round) {
        auto& m = maps[round];
        auto const base = (round % 8) * size;
        for (std::size_t i = 0; i < size; ++i) {
            sink = sink + m.try_emplace(keys[base + i], i).first->second;
        }
        done += size;
    }
#endif

    ioctl(fd_ins, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(fd_cyc, PERF_EVENT_IOC_DISABLE, 0);
    auto const ins = static_cast<double>(read_counter(fd_ins));
    auto const cyc = static_cast<double>(read_counter(fd_cyc));
    std::printf("%s %s %.2f %.2f\n", mode, key_name, ins / static_cast<double>(done), cyc / static_cast<double>(done));
    return 0;
}
