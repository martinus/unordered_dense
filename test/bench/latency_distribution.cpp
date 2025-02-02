#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/print.h>
#include <limits>

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered_map.hpp>

#include <fmt/ostream.h>
#include <fmt/ranges.h>
#include <third-party/nanobench.h>

#include <algorithm>
#include <fstream>
#include <unordered_map>

#include <x86intrin.h> // For RDTSC intrinsics

#if 0

inline uint64_t measure() {
    _mm_lfence(); // Ensure no reordering
    uint64_t tsc = __rdtsc();
    _mm_lfence(); // Ensure no reordering
    return tsc;
}

#else

#    include <linux/perf_event.h>
#    include <sys/ioctl.h>
#    include <sys/syscall.h>
#    include <unistd.h>

static int perf_fd;

void setup_perf() {
    struct perf_event_attr attr = {};
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = sizeof(attr);
    attr.config = PERF_COUNT_HW_INSTRUCTIONS;
    attr.disabled = 1;
    attr.exclude_kernel = 1; // Exclude kernel time if needed

    perf_fd = static_cast<int>(syscall(SYS_perf_event_open, &attr, 0, -1, -1, 0));
    if (perf_fd == -1) {
        perror("perf_event_open failed");
        exit(1);
    }

    ioctl(perf_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(perf_fd, PERF_EVENT_IOC_ENABLE, 0);
}

uint64_t measure() {
    uint64_t ns;
    read(perf_fd, &ns, sizeof(ns));
    return ns;
}

/*
inline uint64_t measure() {
    return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}
*/

#endif

TEST_CASE("bench_latency" * doctest::test_suite("bench") * doctest::skip()) {
    setup_perf();
    // using map_t = ankerl::unordered_dense::map<uint64_t, uint64_t>;
    // using map_t = std::unordered_map<uint64_t, uint64_t>;
    // using map_t = boost::unordered_flat_map<uint64_t, uint64_t>;
    using map_t = boost::unordered_map<uint64_t, uint64_t>;

    static constexpr auto num_elements = size_t(16383);
    auto num_evaluations = size_t(1000000);
    auto measurements = std::vector<uint64_t>(num_evaluations);
    auto best_measurements = std::vector<uint64_t>(1, std::numeric_limits<uint64_t>::max());

    // we just assume that both array only contain unique elements

    auto rng = ankerl::nanobench::Rng(123);

    auto map = map_t();

    // do it several times, so we have some warmup
    for (size_t retries = 0; retries < 2; ++retries) {
        for (size_t eval = 0; eval < num_evaluations; ++eval) {
            auto before = measure();
            map.emplace(rng() % num_elements, 0);
            map.erase(rng() % num_elements);
            auto after = measure();

            measurements[eval] = after - before;
        }

        std::sort(measurements.begin(), measurements.end());
        if (measurements.back() < best_measurements.back()) {
            best_measurements = measurements;
        }
        test::print("min: {}, median: {}, max: {}\n",
                    best_measurements.front(),
                    best_measurements[best_measurements.size() / 2],
                    best_measurements.back());
    }

    auto fout = std::ofstream("times.dat");
    fmt::print(fout, "{}", fmt::join(best_measurements, "\n"));
}
