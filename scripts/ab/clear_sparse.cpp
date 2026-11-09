// What clearing costs on a table that holds few entries against the groups it has (#381). One key
// kind per binary, one header per binary (scripts/ab/clear_sparse.sh builds REV's header, the
// working tree's, and the working tree's with every clear taken per entry). A header that declares
// clear_mode is cleared with clear(clear_mode::sparse) (the script sets AB_CLEAR_SPARSE=1 for it),
// any other with clear():
//
//   KIND_U64    map<uint64_t, uint64_t>, random keys
//   KIND_S16    map<std::string, uint64_t>, 16 byte keys
//   KIND_S135   135 byte keys
//   KIND_S1K    1024 byte keys
//
// The table is reserve()d to PEAK, which leaves its array exactly as a table that grew to PEAK and
// was cleared, then each sample fills N keys and clears. N = groups / RATIO. Three measurements:
//
//   hot    the clear alone, right after the fill, so its groups and keys are in cache
//   cold   the clear alone, after streaming a 256 MB buffer through the cache
//   loop   N inserts and a clear, back to back, timed over enough rounds for >= 20 ms
//
// hot and cold time each clear on its own (steady_clock; its floor is printed as `floor`) and
// print the median and the quartiles of SAMPLES samples; loop prints the median of 7 batches, per
// round. Every sample draws a fresh window of a shuffled pool of 2N + 1024 keys, so the sequence of
// keys is never replayed. Times in ns.
//
//   ./clear_sparse PEAK RATIO MODE SAMPLES
#include <ankerl/unordered_dense.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

template <typename Map>
void clear_map(Map& map) {
#if AB_CLEAR_SPARSE
    map.clear(ankerl::unordered_dense::clear_mode::sparse);
#else
    map.clear();
#endif
}

#if defined(KIND_U64)
using key_type = std::uint64_t;
constexpr char const* kind = "u64";
auto make_key(std::mt19937_64& rng) -> key_type {
    return rng();
}
#else
#    if defined(KIND_S16)
constexpr std::size_t key_len = 16;
constexpr char const* kind = "s16";
#    elif defined(KIND_S135)
constexpr std::size_t key_len = 135;
constexpr char const* kind = "s135";
#    elif defined(KIND_S1K)
constexpr std::size_t key_len = 1024;
constexpr char const* kind = "s1k";
#    else
#        error "define KIND_U64, KIND_S16, KIND_S135 or KIND_S1K"
#    endif
using key_type = std::string;
auto make_key(std::mt19937_64& rng) -> key_type {
    auto s = std::string(key_len, '\0');
    for (std::size_t i = 0; i < key_len; i += 8) {
        auto const r = rng();
        std::memcpy(&s[i], &r, std::min<std::size_t>(8, key_len - i));
    }
    return s;
}
#endif

using map_t = ankerl::unordered_dense::map<key_type, std::uint64_t>;
using clk = std::chrono::steady_clock;

auto ns(clk::duration d) -> double {
    return std::chrono::duration<double, std::nano>(d).count();
}

volatile std::uint64_t sink = 0;

// Streams 256 MB through the cache, writing so that no line stays clean and shared.
void evict(std::vector<std::uint64_t>& buf) {
    auto sum = std::uint64_t{0};
    for (auto& x : buf) {
        sum += x;
        x = sum;
    }
    sink = sink + sum;
}

auto quantile(std::vector<double> v, double q) -> double {
    std::sort(v.begin(), v.end());
    return v[static_cast<std::size_t>(q * static_cast<double>(v.size() - 1))];
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::fprintf(stderr, "usage: %s PEAK RATIO hot|cold|loop SAMPLES\n", argv[0]);
        return 1;
    }
    auto const peak = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
    auto const ratio = static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10));
    auto const mode = std::string(argv[3]);
    auto const samples = static_cast<std::size_t>(std::strtoull(argv[4], nullptr, 10));

    auto map = map_t();
    map.reserve(peak);
    auto const groups = map.bucket_count() / 16;
    auto const n = std::max<std::size_t>(1, groups / ratio);

    auto rng = std::mt19937_64(12345);
    auto pool = std::vector<key_type>();
    pool.reserve((2 * n) + 1024);
    for (std::size_t i = 0; i < (2 * n) + 1024; ++i) {
        pool.push_back(make_key(rng));
    }
    auto fill = [&] {
        auto const start = static_cast<std::size_t>(rng() % (pool.size() - n + 1));
        for (std::size_t i = start; i < start + n; ++i) {
            map.try_emplace(pool[i], i);
        }
    };
    // warm the value vector's capacity and the allocator
    fill();
    clear_map(map);

    auto floor = std::vector<double>();
    for (int i = 0; i < 101; ++i) {
        auto const t0 = clk::now();
        auto const t1 = clk::now();
        floor.push_back(ns(t1 - t0));
    }

    auto v = std::vector<double>();
    if (mode == "hot" || mode == "cold") {
        auto buf = std::vector<std::uint64_t>(mode == "cold" ? (std::size_t{256} << 20U) / 8 : 0, 1);
        for (std::size_t s = 0; s < samples; ++s) {
            fill();
            if (map.size() != n) {
                std::fprintf(stderr, "fill gave %zu, wanted %zu\n", map.size(), n);
                return 1;
            }
            if (!buf.empty()) {
                evict(buf);
            }
            auto const t0 = clk::now();
            clear_map(map);
            auto const t1 = clk::now();
            v.push_back(ns(t1 - t0));
        }
    } else if (mode == "loop") {
        // rounds per batch: about 20 ms, from one probe batch
        auto rounds = std::size_t{16};
        while (true) {
            auto const t0 = clk::now();
            for (std::size_t r = 0; r < rounds; ++r) {
                fill();
                clear_map(map);
            }
            if (ns(clk::now() - t0) > 20e6) {
                break;
            }
            rounds *= 2;
        }
        for (int b = 0; b < 7; ++b) {
            auto const t0 = clk::now();
            for (std::size_t r = 0; r < rounds; ++r) {
                fill();
                clear_map(map);
            }
            v.push_back(ns(clk::now() - t0) / static_cast<double>(rounds));
        }
    } else {
        std::fprintf(stderr, "mode must be hot, cold or loop\n");
        return 1;
    }
    std::printf("%s peak=%zu groups=%zu n=%zu ratio=%zu %s median=%.1f q1=%.1f q3=%.1f floor=%.1f\n",
                kind,
                peak,
                groups,
                n,
                ratio,
                mode.c_str(),
                quantile(v, 0.5),
                quantile(v, 0.25),
                quantile(v, 0.75),
                quantile(floor, 0.5));
    return 0;
}
