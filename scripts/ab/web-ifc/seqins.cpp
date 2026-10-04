// ParseLines' pattern: reserve(n), then map[id] = {type, offset} for ids 1..n in order, then n finds in order.
// std hashes a uint32_t to itself; std+hash gives it this map's hash, map+mul gives this map a plain multiply.
// Each cell is the median of five rounds after one warm-up; the map is destroyed outside the clock.
#include <ankerl/unordered_dense.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <vector>
#if defined(__GLIBC__)
#    include <malloc.h>
#endif

// a plain multiply, the low 64 bits (Fibonacci hashing), declared good enough to use as is
struct mul_hash {
    using is_avalanching = void;
    auto operator()(uint32_t x) const noexcept -> uint64_t {
        return x * UINT64_C(0x9E3779B97F4A7C15);
    }
};

struct line {
    uint32_t type;
    uint32_t offset;
};

static double now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static volatile uint64_t sink = 0;

template <class M>
void run(const char* name, uint32_t n) {
    std::vector<double> ins;
    std::vector<double> fnd;
    for (int round = 0; round < 6; ++round) {
        M m;
        double t0 = now();
        m.reserve(n);
        for (uint32_t i = 1; i <= n; ++i) {
            m[i] = line{i * 7, i};
        }
        double t1 = now();
        uint64_t s = 0;
        for (uint32_t i = 1; i <= n; ++i) {
            s += m.find(i)->second.offset;
        }
        double t2 = now();
        sink = sink + s;
        if (round > 0) { // round 0 is the warm-up
            ins.push_back((t1 - t0) / static_cast<double>(n) * 1e9);
            fnd.push_back((t2 - t1) / static_cast<double>(n) * 1e9);
        }
    }
    std::sort(ins.begin(), ins.end());
    std::sort(fnd.begin(), fnd.end());
    std::printf("%-10s n=%u insert %.1f ns/op  find %.1f ns/op\n", name, n, ins[2], fnd[2]);
}

int main() {
#if defined(__GLIBC__)
    // as test/bench/workloads.h's tame_allocator(): keep large blocks out of mmap so page faults stay out of the clock
    mallopt(M_MMAP_THRESHOLD, 256 * 1024 * 1024);
    mallopt(M_TRIM_THRESHOLD, 256 * 1024 * 1024);
#endif
    for (uint32_t n : {100000U, 1000000U, 3400000U}) {
        run<std::unordered_map<uint32_t, line>>("std", n);
        run<ankerl::unordered_dense::map<uint32_t, line>>("map", n);
        run<ankerl::unordered_dense::map<uint32_t, line, mul_hash>>("map+mul", n);
        run<std::unordered_map<uint32_t, line, ankerl::unordered_dense::hash<uint32_t>>>("std+hash", n);
    }
}
