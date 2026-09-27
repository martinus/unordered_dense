// MySQL 9.7.2's EXCEPT on the map, replayed. Keys are ImmutableStringWithLength: a pointer to a
// one-byte length and eight bytes (the row's hash) in an arena, and only inserted keys are kept
// there. Phase 1 emplaces the first table's rows (key-first where the header allows it), phase 2
// finds the second table's. Counts instructions, cycles and key comparisons per operation per
// phase; one map per binary; MySQL builds with -O2.
//
//   -DDUMP=\"file\"  the keys MySQL really hashed, written by scripts/ab/mysql_except_dump.patch
//                    (one byte insert-or-find, eight bytes key, per call); without it the keys are
//                    splitmix(k) of sysbench's k, uniform in [1, 1M], for both tables
//   -DPOLLUTE=N      N random cache lines of a 64 MB buffer read per operation, the way InnoDB's
//                    scan runs between two of MySQL's map calls
//
//   g++ -O2 -DNDEBUG -std=c++17 -Iinclude [-DDUMP=...] scripts/ab/mysql_except_replay.cpp -o r && taskset -c 2 ./r
#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <linux/perf_event.h>
#include <memory>
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
[[maybe_unused]] auto splitmix(std::uint64_t x) -> std::uint64_t {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31U);
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

std::uint64_t g_equals = 0;
// Other work between two map operations, the way InnoDB's scan runs between two of MySQL's: POLLUTE
// random cache lines of a 64 MB buffer are read per operation. 0 = the map alone.
#ifndef POLLUTE
#    define POLLUTE 0
#endif
std::vector<std::uint64_t> g_junk(std::size_t{64} << 17U);
std::uint64_t g_junk_sum = 0;
rng g_junk_rng{};
inline void pollute() {
    for (int i = 0; i < POLLUTE; ++i)
        g_junk_sum += g_junk[(g_junk_rng() % (g_junk.size() / 8)) * 8];
}
struct istr {
    char const* p;
    [[nodiscard]] auto decode() const -> std::string_view {
        return {p + 1, static_cast<std::size_t>(static_cast<unsigned char>(p[0]))};
    }
};
struct istr_eq {
    auto operator()(istr a, istr b) const -> bool {
        ++g_equals;
        return a.decode() == b.decode();
    }
};
struct istr_hash {
    using is_avalanching = void;
    auto operator()(istr s) const -> std::uint64_t {
        return ankerl::unordered_dense::hash<std::string_view>()(s.decode());
    }
};
struct linked {
    char const* p;
};
using map_t = ankerl::unordered_dense::segmented_map<istr, linked, istr_hash, istr_eq>;

void encode(char* dst, std::uint64_t h) {
    dst[0] = 8;
    std::memcpy(dst + 1, &h, 8);
}

__attribute__((noinline)) std::size_t phase_emplace(map_t& m, std::vector<std::uint64_t> const& rows, char* arena) {
    std::size_t fresh = 0;
    char* top = arena;
    for (auto k : rows) {
        pollute();
        encode(top, k);
        if (m.emplace(istr{top}, linked{nullptr}).second) {
            top += 9;
            ++fresh;
        } // committed only if new
    }
    return fresh;
}
__attribute__((noinline)) std::size_t phase_find(map_t& m, std::vector<std::uint64_t> const& rows) {
    char scratch[16];
    std::size_t found = 0;
    for (auto k : rows) {
        pollute();
        encode(scratch, k);
        found += m.find(istr{scratch}) != m.end() ? 1U : 0U;
    }
    return found;
}
} // namespace
int main() {
    constexpr std::size_t n = 1000000;
    auto left = std::vector<std::uint64_t>(), right = std::vector<std::uint64_t>();
#ifdef DUMP
    {
        FILE* f = std::fopen(DUMP, "rb");
        unsigned char rec[9];
        while (f != nullptr && std::fread(rec, 1, 9, f) == 9) {
            std::uint64_t k = 0;
            std::memcpy(&k, rec + 1, 8);
            (rec[0] != 0 ? left : right).push_back(k);
        }
        if (f != nullptr) {
            std::fclose(f);
        }
    }
    auto const key_of = [](std::uint64_t k) {
        return k;
    };
#else
    auto r = rng{};
    left.resize(n);
    right.resize(n);
    for (auto& v : left)
        v = 1 + r() % n;
    for (auto& v : right)
        v = 1 + r() % n;
    auto const key_of = [](std::uint64_t k) {
        return splitmix(k);
    };
#endif
    for (auto& v : left)
        v = key_of(v);
    for (auto& v : right)
        v = key_of(v);
    auto arena = std::make_unique<char[]>(n * 9 + 16);
    int fi = open_counter(PERF_COUNT_HW_INSTRUCTIONS), fc = open_counter(PERF_COUNT_HW_CPU_CYCLES);
    double res[2][3] = {};
    std::size_t fresh = 0, found = 0;
    constexpr int rounds = 6;
    for (int round = 0; round < rounds; ++round) {
        auto m = map_t();
        for (int ph = 0; ph < 2; ++ph) {
            g_equals = 0;
            ioctl(fi, PERF_EVENT_IOC_RESET, 0);
            ioctl(fc, PERF_EVENT_IOC_RESET, 0);
            ioctl(fi, PERF_EVENT_IOC_ENABLE, 0);
            ioctl(fc, PERF_EVENT_IOC_ENABLE, 0);
            if (ph == 0)
                fresh = phase_emplace(m, left, arena.get());
            else
                found = phase_find(m, right);
            ioctl(fi, PERF_EVENT_IOC_DISABLE, 0);
            ioctl(fc, PERF_EVENT_IOC_DISABLE, 0);
            if (round > 0) {
                res[ph][0] += rd(fi);
                res[ph][1] += rd(fc);
                res[ph][2] += static_cast<double>(g_equals);
            }
        }
    }
    for (int ph = 0; ph < 2; ++ph) {
        std::printf("%-7s instr %7.2f cycles %7.2f compares %.4f  ",
                    ph == 0 ? "emplace" : "find",
                    res[ph][0] / (rounds - 1) / static_cast<double>(ph == 0 ? left.size() : right.size()),
                    res[ph][1] / (rounds - 1) / static_cast<double>(ph == 0 ? left.size() : right.size()),
                    res[ph][2] / (rounds - 1) / static_cast<double>(ph == 0 ? left.size() : right.size()));
    }
    std::printf("junk %llu ", static_cast<unsigned long long>(g_junk_sum & 1U));
    std::printf("new %.3f found %.3f\n",
                static_cast<double>(fresh) / static_cast<double>(left.size()),
                static_cast<double>(found) / static_cast<double>(right.size()));
}
