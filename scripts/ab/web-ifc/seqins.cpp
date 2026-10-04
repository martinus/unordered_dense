// ParseLines' pattern: reserve(n), then map[id] = {type, offset} for ids 1..n in order, then n finds in order.
#include <ankerl/unordered_dense.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <unordered_map>

struct line {
    uint32_t type;
    uint32_t offset;
};
static double now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

template <class M>
void run(const char* name, uint32_t n) {
    M m;
    double t0 = now();
    m.reserve(n);
    for (uint32_t i = 1; i <= n; ++i)
        m[i] = line{i * 7, i};
    double t1 = now();
    uint64_t s = 0;
    for (uint32_t i = 1; i <= n; ++i)
        s += m.find(i)->second.offset;
    double t2 = now();
    std::printf("%-10s n=%u insert %.1f ns/op  find %.1f ns/op  (%llu)\n",
                name,
                n,
                (t1 - t0) / static_cast<double>(n) * 1e9,
                (t2 - t1) / static_cast<double>(n) * 1e9,
                static_cast<unsigned long long>(s));
}
int main() {
    for (uint32_t n : {100000u, 1000000u, 3400000u}) {
        run<std::unordered_map<uint32_t, line>>("std", n);
        run<ankerl::unordered_dense::map<uint32_t, line>>("ud-5.3.1", n);
    }
}
