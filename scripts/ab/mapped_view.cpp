// A map_view over a mapped file against the owning map, on 4 KB and on 2 MB pages (#301).
//
//   mapped_view gen  <file> <n>                 write map<uint64_t, uint64_t> of n entries to <file>
//   mapped_view warm <file> [rounds]            lookups after a warm-up pass, per round
//   mapped_view cold <file> [drop]              the first 100000 lookups after loading; `drop`
//                                               first evicts the file from the page cache
//   mapped_view shared <file>                   two processes, each loads and looks up every key
//
// One mode per binary (-DUDM_MODE_<mode>=1; notes: "The harness had to be pinned first"):
//
//   owning         read() the file into a vector and a block array, then map(values, index,
//                  trust::unchecked): what a caller who copies the snapshot holds
//   owning_huge    the same into huge_page::map (#271): both arrays on MADV_HUGEPAGE mappings
//   view_file      map_view over mmap(PROT_READ, MAP_PRIVATE) of the file: page cache, 4 KB pages
//   view_populate  the same with MAP_POPULATE: separates first touch from page size
//   view_collapse  the file mapping with MADV_HUGEPAGE and MADV_COLLAPSE: page cache on 2 MB pages,
//                  if the kernel and the filesystem have it (reported: FilePmdMapped)
//   view_thp_copy  the file's bytes copied into an anonymous 2 MB-aligned MADV_HUGEPAGE mapping:
//                  the proxy for hugetlbfs where no huge pages are reserved (reported: AnonHugePages)
//   view_hugetlb   the same copy into anonymous MAP_HUGETLB memory; prints `unavailable` when
//                  /proc/sys/vm/nr_hugepages has none to give
//
// Lookups are all hits, the key drawn from an rng and computed from its number, not read from an
// array: throughput, several lookups in flight (notes: "measure throughput"). cycles and the two TLB
// counters are read with perf_event_open around the timed loop only.
//
// The file: a 4096 byte header, then the values at 2 MB, then the index at the next 2 MB boundary,
// so that a mapping can put either array on huge pages.
#include "max_rss.h"

#include <ankerl/huge_page_allocator.h>
#include <ankerl/unordered_dense.h>
#include <bench/workloads.h>
#include <third-party/nanobench.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <fcntl.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#if !defined(MADV_COLLAPSE)
#    define MADV_COLLAPSE 25
#endif

namespace {

namespace ud = ankerl::unordered_dense;

using map_t = ud::map<std::uint64_t, std::uint64_t>;
using huge_map_t = ud::huge_page::map<std::uint64_t, std::uint64_t>;
using view_t = map_t::view_type;
using value_t = map_t::value_type;
using block_t = map_t::index_block;
using clock_t_ = std::chrono::steady_clock;

constexpr std::size_t huge = std::size_t{2} << 20U;
constexpr std::uint64_t magic = 0x3130337765697670U; // "pview301"
constexpr std::size_t lookups_per_round = 1000000;
constexpr std::size_t cold_lookups = 100000;

auto round_up(std::size_t x, std::size_t to) -> std::size_t {
    return (x + to - 1) / to * to;
}

struct header {
    std::uint64_t magic;
    std::uint64_t format_id;
    std::uint64_t num_values;
    std::uint64_t num_blocks;
    std::uint64_t values_offset;
    std::uint64_t index_offset;
    std::uint64_t file_size;
};

auto key_of(std::uint64_t i) -> std::uint64_t {
    return workloads::key_source<std::uint64_t>::get(i);
}

auto read_header(char const* path) -> header {
    auto h = header{};
    auto const fd = open(path, O_RDONLY);
    if (fd < 0 || pread(fd, &h, sizeof(h), 0) != static_cast<ssize_t>(sizeof(h)) || h.magic != magic ||
        h.format_id != map_t::index_format_id) {
        std::fprintf(stderr, "%s: not a file from this binary's gen\n", path);
        std::exit(2);
    }
    close(fd);
    return h;
}

void read_exact(int fd, void* to, std::size_t bytes, std::size_t offset) {
    auto* p = static_cast<char*>(to);
    while (bytes != 0) {
        auto const got = pread(fd, p, std::min(bytes, std::size_t{1} << 30U), static_cast<off_t>(offset));
        if (got <= 0) {
            std::perror("pread");
            std::exit(2);
        }
        p += got;
        offset += static_cast<std::size_t>(got);
        bytes -= static_cast<std::size_t>(got);
    }
}

void write_exact(int fd, void const* from, std::size_t bytes, std::size_t offset) {
    auto const* p = static_cast<char const*>(from);
    while (bytes != 0) {
        auto const put = pwrite(fd, p, std::min(bytes, std::size_t{1} << 30U), static_cast<off_t>(offset));
        if (put <= 0) {
            std::perror("pwrite");
            std::exit(2);
        }
        p += put;
        offset += static_cast<std::size_t>(put);
        bytes -= static_cast<std::size_t>(put);
    }
}

auto gen(char const* path, std::size_t n) -> int {
    auto m = map_t();
    m.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        m.try_emplace(key_of(i), i);
    }
    auto h = header{};
    h.magic = magic;
    h.format_id = map_t::index_format_id;
    h.num_values = m.size();
    h.num_blocks = m.index().size();
    h.values_offset = huge;
    h.index_offset = round_up(h.values_offset + (h.num_values * sizeof(value_t)), huge);
    h.file_size = round_up(h.index_offset + (h.num_blocks * sizeof(block_t)), huge);
    auto const tmp = std::string(path) + ".tmp";
    auto const fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || ftruncate(fd, static_cast<off_t>(h.file_size)) != 0) {
        std::perror(path);
        return 2;
    }
    write_exact(fd, &h, sizeof(h), 0);
    write_exact(fd, m.values().data(), h.num_values * sizeof(value_t), h.values_offset);
    write_exact(fd, m.index().data(), h.num_blocks * sizeof(block_t), h.index_offset);
    fsync(fd);
    close(fd);
    // written beside it and renamed into place, which is the contract mapped_view.h documents
    if (rename(tmp.c_str(), path) != 0) {
        std::perror("rename");
        return 2;
    }
    std::printf("%s n %zu values %zu MB index %zu MB file %zu MB\n",
                path,
                n,
                (h.num_values * sizeof(value_t)) >> 20U,
                (h.num_blocks * sizeof(block_t)) >> 20U,
                static_cast<std::size_t>(h.file_size) >> 20U);
    return 0;
}

// What one load holds: the table, and whatever memory it reads.
struct loaded {
    void* region = MAP_FAILED;
    std::size_t region_size = 0;
    std::optional<map_t> owning;
    std::optional<huge_map_t> owning_huge;
    std::optional<view_t> view;
    loaded() = default;
    loaded(loaded const&) = delete;
    auto operator=(loaded const&) -> loaded& = delete;
    ~loaded() {
        if (region != MAP_FAILED) {
            munmap(region, region_size);
        }
    }
    template <typename F>
    auto apply(F&& f) -> decltype(auto) {
#if UDM_MODE_owning
        return f(*owning);
#elif UDM_MODE_owning_huge
        return f(*owning_huge);
#else
        return f(*view);
#endif
    }
};

template <typename Map>
auto read_owning(int fd, header const& h) -> Map {
    auto values = typename Map::value_container_type();
    values.resize(h.num_values);
    read_exact(fd, values.data(), h.num_values * sizeof(value_t), h.values_offset);
    auto blocks = std::vector<block_t>(h.num_blocks);
    read_exact(fd, blocks.data(), h.num_blocks * sizeof(block_t), h.index_offset);
    return Map(std::move(values), {blocks.data(), blocks.size()}, ud::trust::unchecked);
}

// An anonymous region aligned to 2 MB, `bytes` rounded up to 2 MB.
auto anonymous_huge(std::size_t bytes, int extra_flags) -> void* {
#if UDM_MODE_view_hugetlb
    return mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | extra_flags, -1, 0);
#else
    static_cast<void>(extra_flags);
    auto* const raw = mmap(nullptr, bytes + huge, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) {
        return raw;
    }
    auto const addr = reinterpret_cast<std::uintptr_t>(raw);
    auto const aligned = round_up(addr, huge);
    if (aligned != addr) {
        munmap(raw, aligned - addr);
    }
    munmap(reinterpret_cast<void*>(aligned + bytes), huge - (aligned - addr));
    madvise(reinterpret_cast<void*>(aligned), bytes, MADV_HUGEPAGE);
    return reinterpret_cast<void*>(aligned);
#endif
}

auto load(char const* path, header const& h, loaded& out, [[maybe_unused]] bool shared) -> bool {
    auto const fd = open(path, O_RDONLY);
    if (fd < 0) {
        std::perror(path);
        std::exit(2);
    }
    auto const bytes = static_cast<std::size_t>(h.file_size);
#if UDM_MODE_owning
    out.owning.emplace(read_owning<map_t>(fd, h));
#elif UDM_MODE_owning_huge
    out.owning_huge.emplace(read_owning<huge_map_t>(fd, h));
#elif UDM_MODE_view_file || UDM_MODE_view_populate || UDM_MODE_view_collapse
    auto flags = shared ? MAP_SHARED : MAP_PRIVATE;
#    if UDM_MODE_view_populate
    flags |= MAP_POPULATE;
#    endif
    // A 2 MB-aligned address, so that a file offset at a 2 MB boundary can be PMD-mapped.
    auto* const hint = anonymous_huge(bytes, 0);
    out.region = mmap(hint, bytes, PROT_READ, flags | MAP_FIXED, fd, 0);
    out.region_size = bytes;
#    if UDM_MODE_view_collapse
    madvise(out.region, bytes, MADV_HUGEPAGE);
    if (madvise(out.region, bytes, MADV_COLLAPSE) != 0) {
        static bool said = false;
        if (!said) {
            std::fprintf(stderr, "MADV_COLLAPSE on the file mapping: %s\n", std::strerror(errno));
            said = true;
        }
    }
#    endif
#elif UDM_MODE_view_thp_copy || UDM_MODE_view_hugetlb
    out.region = anonymous_huge(bytes, MAP_HUGETLB);
    out.region_size = bytes;
    if (out.region == MAP_FAILED) {
        close(fd);
        return false;
    }
    read_exact(fd, out.region, bytes, 0);
    mprotect(out.region, bytes, PROT_READ);
#else
#    error "set -DUDM_MODE_<mode>=1"
#endif
#if !UDM_MODE_owning && !UDM_MODE_owning_huge
    if (out.region == MAP_FAILED) {
        std::perror("mmap");
        std::exit(2);
    }
    auto const* base = static_cast<char const*>(out.region);
    out.view.emplace(view_t::value_container_type(reinterpret_cast<value_t const*>(base + h.values_offset), h.num_values),
                     map_t::index_view(reinterpret_cast<block_t const*>(base + h.index_offset), h.num_blocks),
                     ud::trust::unchecked);
#endif
    close(fd);
    return true;
}

template <typename Map>
auto lookups(Map const& m, ankerl::nanobench::Rng& rng, std::size_t count, std::uint64_t n) -> std::uint64_t {
    auto const end = m.end();
    auto checksum = std::uint64_t{0};
    for (std::size_t i = 0; i < count; ++i) {
        auto const it = m.find(key_of(((rng() >> 32U) * n) >> 32U));
        if (it != end) {
            checksum += it->second;
        }
    }
    return checksum;
}

// cycles, L1 dTLB misses (ls_l1_d_tlb_miss.all) and the ones that also miss the L2 TLB and walk the
// page table (ls_l1_d_tlb_miss.all_l2_miss), as one group: three events, no multiplexing.
struct counters {
    std::array<int, 3> fd{-1, -1, -1};
    counters() {
        auto open_one = [](std::uint32_t type, std::uint64_t config, int group) {
            auto a = perf_event_attr{};
            a.size = sizeof(a);
            a.type = type;
            a.config = config;
            a.disabled = group < 0 ? 1 : 0;
            a.exclude_kernel = 1;
            a.exclude_hv = 1;
            a.read_format = PERF_FORMAT_GROUP;
            return static_cast<int>(syscall(SYS_perf_event_open, &a, 0, -1, group, 0));
        };
        fd[0] = open_one(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, -1);
        fd[1] = open_one(PERF_TYPE_RAW, 0xff45, fd[0]);
        fd[2] = open_one(PERF_TYPE_RAW, 0xf045, fd[0]);
    }
    counters(counters const&) = delete;
    auto operator=(counters const&) -> counters& = delete;
    ~counters() {
        for (auto f : fd) {
            if (f >= 0) {
                close(f);
            }
        }
    }
    void start() const {
        ioctl(fd[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
        ioctl(fd[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    }
    [[nodiscard]] auto stop() const -> std::array<double, 3> {
        ioctl(fd[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
        auto buf = std::array<std::uint64_t, 4>{};
        if (fd[0] < 0 || read(fd[0], buf.data(), sizeof(buf)) < 0) {
            return {-1, -1, -1};
        }
        return {static_cast<double>(buf[1]), static_cast<double>(buf[2]), static_cast<double>(buf[3])};
    }
};

auto smaps_kb(char const* field) -> long {
    return max_rss::status_kb(field, "/proc/self/smaps_rollup");
}

auto median(std::vector<double> v) -> double {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

auto warm(char const* path, std::size_t rounds) -> int {
    auto const h = read_header(path);
    auto l = loaded();
    if (!load(path, h, l, false)) {
        std::printf("%zu unavailable\n", static_cast<std::size_t>(h.num_values));
        return 0;
    }
    auto rng = ankerl::nanobench::Rng(7);
    auto sink = std::uint64_t{0};
    // one pass over every key first: the pages are faulted in and the caches hold what they will
    l.apply([&](auto const& m) {
        for (std::uint64_t i = 0; i < h.num_values; ++i) {
            sink += m.find(key_of(i))->second;
        }
    });
    auto const anon_huge_kb = smaps_kb("AnonHugePages:");
    auto const file_pmd_kb = smaps_kb("FilePmdMapped:");
    auto const c = counters();
    auto ns = std::vector<double>();
    auto cyc = std::vector<double>();
    auto l1 = std::vector<double>();
    auto walks = std::vector<double>();
    for (std::size_t r = 0; r < rounds; ++r) {
        c.start();
        auto const t0 = clock_t_::now();
        sink += l.apply([&](auto const& m) {
            return lookups(m, rng, lookups_per_round, h.num_values);
        });
        auto const t1 = clock_t_::now();
        auto const v = c.stop();
        ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / lookups_per_round);
        cyc.push_back(v[0] / lookups_per_round);
        l1.push_back(v[1] / lookups_per_round);
        walks.push_back(v[2] / lookups_per_round);
    }
    ankerl::nanobench::doNotOptimizeAway(sink);
    std::printf("%zu ns/lookup %.2f cycles %.1f l1_dtlb_miss %.3f page_walks %.3f anon_huge_MB %ld file_pmd_MB %ld\n",
                static_cast<std::size_t>(h.num_values),
                median(ns),
                median(cyc),
                median(l1),
                median(walks),
                anon_huge_kb >> 10,
                file_pmd_kb >> 10);
    return 0;
}

auto faults() -> std::array<long, 2> {
    auto u = rusage{};
    getrusage(RUSAGE_SELF, &u);
    return {u.ru_minflt, u.ru_majflt};
}

auto cold(char const* path, bool drop) -> int {
    auto const h = read_header(path);
    if (drop) {
        auto const fd = open(path, O_RDONLY);
        posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
        close(fd);
    }
    auto l = loaded();
    auto const f0 = faults();
    auto const t0 = clock_t_::now();
    if (!load(path, h, l, false)) {
        std::printf("%zu unavailable\n", static_cast<std::size_t>(h.num_values));
        return 0;
    }
    auto const t1 = clock_t_::now();
    auto const f1 = faults();
    auto rng = ankerl::nanobench::Rng(11);
    auto const sink = l.apply([&](auto const& m) {
        return lookups(m, rng, cold_lookups, h.num_values);
    });
    auto const t2 = clock_t_::now();
    auto const f2 = faults();
    ankerl::nanobench::doNotOptimizeAway(sink);
    auto ms = [](auto a, auto b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    std::printf("%zu load_ms %.2f first_100k_ms %.2f total_ms %.2f load_faults %ld/%ld lookup_faults %ld/%ld\n",
                static_cast<std::size_t>(h.num_values),
                ms(t0, t1),
                ms(t1, t2),
                ms(t0, t2),
                f1[0] - f0[0],
                f1[1] - f0[1],
                f2[0] - f1[0],
                f2[1] - f1[1]);
    return 0;
}

// Resident pages of the file in the page cache, from mincore over a fresh mapping (which itself
// faults nothing in).
auto page_cache_mb(char const* path) -> long {
    auto const fd = open(path, O_RDONLY);
    auto st = (struct stat){};
    fstat(fd, &st);
    auto const bytes = static_cast<std::size_t>(st.st_size);
    auto* const p = mmap(nullptr, bytes, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    auto const page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto vec = std::vector<unsigned char>((bytes + page - 1) / page);
    mincore(p, bytes, vec.data());
    munmap(p, bytes);
    auto resident = std::size_t{0};
    for (auto c : vec) {
        resident += c & 1U;
    }
    return static_cast<long>((resident * page) >> 20U);
}

// Two processes load the file the mode's way and look up every key, then each reports its RSS,
// the part of it that is file pages, and its PSS (shared pages split between their users), while
// both still hold their mapping.
auto shared(char const* path) -> int {
    auto const h = read_header(path);
    {
        auto const fd = open(path, O_RDONLY);
        posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
        close(fd);
    }
    int ready[2];
    int go[2];
    int out[2];
    if (pipe(ready) != 0 || pipe(go) != 0 || pipe(out) != 0) {
        return 2;
    }
    constexpr int procs = 2;
    for (int p = 0; p < procs; ++p) {
        if (fork() == 0) {
            auto l = loaded();
            auto c = 'r';
            if (!load(path, h, l, true)) {
                std::array<long, 3> v{-1, -1, -1};
                static_cast<void>(write(ready[1], &c, 1));
                static_cast<void>(read(go[0], &c, 1));
                static_cast<void>(write(out[1], v.data(), sizeof(v)));
                static_cast<void>(read(go[0], &c, 1));
                _exit(0);
            }
            auto sink = l.apply([&](auto const& m) {
                auto s = std::uint64_t{0};
                for (std::uint64_t i = 0; i < h.num_values; ++i) {
                    s += m.find(key_of(i))->second;
                }
                return s;
            });
            ankerl::nanobench::doNotOptimizeAway(sink);
            static_cast<void>(write(ready[1], &c, 1));
            static_cast<void>(read(go[0], &c, 1)); // both are loaded: now read the numbers
            std::array<long, 3> v{max_rss::status_kb("VmRSS:"), max_rss::status_kb("RssFile:"), smaps_kb("Pss:")};
            static_cast<void>(write(out[1], v.data(), sizeof(v)));
            static_cast<void>(read(go[0], &c, 1)); // and hold the mapping until the parent has looked
            _exit(0);
        }
    }
    for (int p = 0; p < procs; ++p) {
        auto c = char{};
        static_cast<void>(read(ready[0], &c, 1));
    }
    auto const cache = page_cache_mb(path);
    auto const go_twice = std::array<char, procs>{};
    static_cast<void>(write(go[1], go_twice.data(), procs));
    std::printf("%zu", static_cast<std::size_t>(h.num_values));
    for (int p = 0; p < procs; ++p) {
        auto v = std::array<long, 3>{};
        static_cast<void>(read(out[0], v.data(), sizeof(v)));
        std::printf(" proc%d rss_MB %ld file_MB %ld pss_MB %ld", p, v[0] >> 10, v[1] >> 10, v[2] >> 10);
    }
    static_cast<void>(write(go[1], go_twice.data(), procs));
    for (int p = 0; p < procs; ++p) {
        wait(nullptr);
    }
    std::printf(" page_cache_MB %ld\n", cache);
    return 0;
}

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 3) {
        std::printf("usage: %s gen|warm|cold|shared <file> [n|rounds|drop]\n", argv[0]);
        return 1;
    }
    auto const what = std::string(argv[1]);
    if (what == "gen") {
        return gen(argv[2], argc > 3 ? static_cast<std::size_t>(std::strtoull(argv[3], nullptr, 10)) : 1000000);
    }
    if (what == "warm") {
        return warm(argv[2], argc > 3 ? static_cast<std::size_t>(std::strtoull(argv[3], nullptr, 10)) : 7);
    }
    if (what == "cold") {
        return cold(argv[2], argc > 3 && std::string(argv[3]) == "drop");
    }
    if (what == "shared") {
        return shared(argv[2]);
    }
    return 1;
}
