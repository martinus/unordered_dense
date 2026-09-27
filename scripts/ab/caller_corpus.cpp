// Caller loops the way programs write them, each guarding a slowdown that happened (#310, #311,
// #312, #321, MySQL). One loop per binary, picked with -DLOOP_<name>, so that nothing else competes
// for the inliner's budget: what is judged here is how the map's code sits in a caller's loop, and
// that is decided per function. ./caller_corpus N prints: loop N instr/op cycles/op, counted around
// the loop only.
//
// Several loops pick their key with `r() % n` and store into what they found. That is on purpose:
// on Zen 4 a loop variable stored and reloaded every iteration, a store whose address comes from a
// cache miss, and a division on the way to the next key stop a loop overlapping its misses
// (scripts/ab/spill_trap.cpp, notes "stored and reloaded"), and whether a caller's variables spill
// depends on how much of the map is inlined into it. Programs write loops like these.
//
// scripts/ab/caller_corpus.sh builds and runs every loop for several headers and compilers.
#include <ankerl/unordered_dense.h>

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
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

auto read_counter(int fd) -> double {
    std::uint64_t v = 0;
    return read(fd, &v, sizeof(v)) == sizeof(v) ? static_cast<double>(v) : 0.0;
}

struct counters {
    int ins = open_counter(PERF_COUNT_HW_INSTRUCTIONS);
    int cyc = open_counter(PERF_COUNT_HW_CPU_CYCLES);
    void start() const {
        ioctl(ins, PERF_EVENT_IOC_RESET, 0);
        ioctl(cyc, PERF_EVENT_IOC_RESET, 0);
        ioctl(ins, PERF_EVENT_IOC_ENABLE, 0);
        ioctl(cyc, PERF_EVENT_IOC_ENABLE, 0);
    }
    void stop() const {
        ioctl(ins, PERF_EVENT_IOC_DISABLE, 0);
        ioctl(cyc, PERF_EVENT_IOC_DISABLE, 0);
    }
};

struct rng {
    std::uint64_t s = 0x243f6a8885a308d3ULL;
    auto operator()() -> std::uint64_t {
        s ^= s << 13U;
        s ^= s >> 7U;
        s ^= s << 17U;
        return s;
    }
};

// A bijection, so that the keys of a table of n are f(0) .. f(n - 1) and no key array is read.
[[maybe_unused]] auto f(std::uint64_t i) -> std::uint64_t {
    i *= 0x9E3779B97F4A7C15ULL;
    return i ^ (i >> 29U);
}

auto splitmix64(std::uint64_t* x) -> std::uint64_t {
    std::uint64_t z = ((*x) += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31U);
}

volatile std::uint64_t g_sink = 0;

using map64 = ankerl::unordered_dense::map<std::uint64_t, std::uint64_t>;

// #310: udb3's loops (attractivechaos/udb3, test.cpp), its non-avalanching hash, the map a local.
struct Hash32 {
    auto operator()(std::uint32_t x) const -> std::size_t {
        std::uint64_t y = x;
        y = (y ^ (y >> 16U)) * 0x45d9f3bU;
        return static_cast<std::size_t>(y ^ (y >> 16U));
    }
};

[[maybe_unused]] __attribute__((noinline)) auto udb3(std::uint32_t total, std::uint32_t n, bool is_del) -> std::uint64_t {
    ankerl::unordered_dense::map<std::uint32_t, std::uint32_t, Hash32> h;
    std::uint64_t z = 0;
    std::uint64_t x = 1;
    for (std::uint32_t i = 0; i < total; ++i) {
        std::uint64_t const y = splitmix64(&x);
        auto const key = static_cast<std::uint32_t>(y % (n >> 2U)) * 0x45D9F3BU;
        if (is_del) {
            auto p = h.try_emplace(key, i);
            if (!p.second) {
                h.erase(p.first);
            } else {
                ++z;
            }
        } else {
            z += ++h[key];
        }
    }
    return z + h.size();
}

// #311: a 12 byte key built field by field right before the lookup, hashed through memory.
struct coord {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
    auto operator==(coord const& o) const -> bool {
        return x == o.x && y == o.y && z == o.z;
    }
};
struct coord_hash {
    using is_avalanching = void;
    auto operator()(coord const& c) const -> std::uint64_t {
        // hash_bytes over the key's memory, through the public entry point that every 5.x has
        return ankerl::unordered_dense::hash<std::string_view>()(
            std::string_view(reinterpret_cast<char const*>(&c), sizeof(c)));
    }
};

// MySQL 9.7.2's set operations: an ImmutableStringWithLength key, a pointer to a length byte and
// the bytes, in an arena.
struct istr {
    char const* p;
    [[nodiscard]] auto decode() const -> std::string_view {
        return {p + 1, static_cast<std::size_t>(static_cast<unsigned char>(p[0]))};
    }
    auto operator==(istr const& o) const -> bool {
        return decode() == o.decode();
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

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s N (N a power of two)\n", argv[0]);
        return 1;
    }
    std::uint64_t const n = std::strtoull(argv[1], nullptr, 10);
    auto const c = counters{};
    [[maybe_unused]] auto r = rng{};
    std::uint64_t acc = 0;
    std::uint64_t ops = 0;
    char const* name = "?";

#if defined(LOOP_udb3_insert)
    // #310, clang: ++h[key] with the key from a modulo, 5n inputs over n/4... n distinct keys.
    name = "udb3_insert";
    ops = 5 * n;
    c.start();
    acc = udb3(static_cast<std::uint32_t>(ops), static_cast<std::uint32_t>(4 * n), false);
    c.stop();
#elif defined(LOOP_udb3_del)
    // #310, gcc: try_emplace, erase if it was there.
    name = "udb3_del";
    ops = 5 * n;
    c.start();
    acc = udb3(static_cast<std::uint32_t>(ops), static_cast<std::uint32_t>(4 * n), true);
    c.stop();
#elif defined(LOOP_bump_mod)
    // ++m[k] on present keys picked with a modulo: the spill trap in its plainest form.
    name = "bump_mod";
    auto m = map64();
    for (std::uint64_t i = 0; i < n; ++i) {
        m.try_emplace(f(i), i);
    }
    ops = 4 * n;
    c.start();
    for (std::uint64_t j = 0; j < ops; ++j) {
        acc += ++m[f(r() % n)];
    }
    c.stop();
#elif defined(LOOP_bump_mask)
    // #321: ++m[k] on present keys picked with a mask, the counting loop of an aggregation.
    name = "bump_mask";
    auto m = map64();
    for (std::uint64_t i = 0; i < n; ++i) {
        m.try_emplace(f(i), i);
    }
    ops = 4 * n;
    c.start();
    for (std::uint64_t j = 0; j < ops; ++j) {
        acc += ++m[f(r() & (n - 1))];
    }
    c.stop();
#elif defined(LOOP_churn_mod)
    // try_emplace, erase if it was there, keys from 2n with a modulo: the table stays about n.
    name = "churn_mod";
    auto m = map64();
    for (std::uint64_t i = 0; i < 2 * n; i += 2) {
        m.try_emplace(f(i), i);
    }
    ops = 4 * n;
    c.start();
    for (std::uint64_t j = 0; j < ops; ++j) {
        auto p = m.try_emplace(f(r() % (2 * n)), j);
        if (!p.second) {
            m.erase(p.first);
        } else {
            ++acc;
        }
    }
    c.stop();
#elif defined(LOOP_build)
    // A build from empty, growing, no division.
    name = "build";
    ops = n;
    c.start();
    {
        auto m = map64();
        for (std::uint64_t i = 0; i < n; ++i) {
            acc += ++m[f(i)];
        }
        acc += m.size();
    }
    c.stop();
#elif defined(LOOP_string_hit)
    // #321, string keys: ++m[s] on present keys. The keys are read from an array, which a string
    // key cannot avoid; the array is the same for every header.
    name = "string_hit";
    auto keys = std::vector<std::string>();
    auto m = ankerl::unordered_dense::map<std::string, std::uint64_t>();
    for (std::uint64_t i = 0; i < n; ++i) {
        auto k = std::to_string(f(i));
        m.try_emplace(k, i);
        keys.push_back(std::move(k));
    }
    ops = 4 * n;
    c.start();
    for (std::uint64_t j = 0; j < ops; ++j) {
        acc += ++m[keys[r() & (n - 1)]];
    }
    c.stop();
#elif defined(LOOP_struct_key)
    // #311 and #312: segmented_map with a 12 byte key built field by field right before the lookup
    // and hashed through memory, as Bonxai's root map does.
    name = "struct_key";
    auto m = ankerl::unordered_dense::segmented_map<coord, std::uint64_t, coord_hash>();
    for (std::uint64_t i = 0; i < n; ++i) {
        auto const v = f(i);
        m.try_emplace(coord{static_cast<std::int32_t>(v & 0xfffffU),
                            static_cast<std::int32_t>((v >> 20U) & 0xfffffU),
                            static_cast<std::int32_t>(v >> 40U)},
                      i);
    }
    ops = 4 * n;
    c.start();
    for (std::uint64_t j = 0; j < ops; ++j) {
        auto const v = f(r() & (n - 1));
        auto key = coord{};
        key.x = static_cast<std::int32_t>(v & 0xfffffU);
        key.y = static_cast<std::int32_t>((v >> 20U) & 0xfffffU);
        key.z = static_cast<std::int32_t>(v >> 40U);
        auto it = m.find(key);
        acc += it == m.end() ? 0 : ++it->second;
    }
    c.stop();
#elif defined(LOOP_mysql_except)
    // MySQL's EXCEPT: emplace(key, mapped) key-first over the first table, find() over the second.
    name = "mysql_except";
    auto arena = std::vector<char>((n + 1) * 9);
    auto m = ankerl::unordered_dense::segmented_map<istr, linked, istr_hash>();
    ops = 2 * n;
    char scratch[16];
    c.start();
    char* top = arena.data();
    for (std::uint64_t j = 0; j < n; ++j) {
        auto const k = f(r() % n);
        top[0] = 8;
        std::memcpy(top + 1, &k, 8);
        if (m.emplace(istr{top}, linked{nullptr}).second) {
            top += 9;
        }
    }
    for (std::uint64_t j = 0; j < n; ++j) {
        auto const k = f(r() % n);
        scratch[0] = 8;
        std::memcpy(scratch + 1, &k, 8);
        acc += m.find(istr{scratch}) != m.end() ? 1U : 0U;
    }
    c.stop();
#else
#    error "define one LOOP_<name>"
#endif

    g_sink = acc;
    std::printf("%s %llu %.2f %.2f\n",
                name,
                static_cast<unsigned long long>(n),
                read_counter(c.ins) / static_cast<double>(ops),
                read_counter(c.cyc) / static_cast<double>(ops));
    return 0;
}
