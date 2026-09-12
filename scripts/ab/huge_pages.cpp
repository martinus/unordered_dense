// What the huge page allocator is worth, across the size axis and per workload.
//
// The scored benchmark's workloads, instantiated on the map with std::allocator and with
// huge_page_allocator, one cell per run so that the variant is a template instantiation and never a
// branch inside anything timed. The workloads are the score's own (test/bench/workloads.h), so a
// cell here is the same code the score runs, at a size the score does not.
//
//   huge_pages <std|huge|huge4|seg|seghuge|seghuge16|boost|boosthuge> <u64|str|big> <build|churn|find|ie> <size>
//
// prints ns per operation and the operation count, where an operation is what the workload does
// per element: one insert for build, erase + insert + two finds per churn round, one hundred finds
// per step of find, an operator[] and an erase per step of insert-erase. `huge4` is the allocator
// with a 4 MB threshold, which #231 named as the starting point; `huge` is the 2 MB default. `seg*`
// is the segmented container in the map's container slot, `boost*` is boost::unordered_flat_map
// with the same hash, both compiled in only where their headers are.
//
// Sizes are template parameters of the workloads, so the set is fixed here: 50000, 200000, 800000,
// 2000000, 4000000. The first is the score's own churn size and the smallest table whose blocks
// can reach the 2 MB threshold at all; the last is where the 2026-09-06 lookup measurement found 22%.
#include <ankerl/huge_page_allocator.h>
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#if defined(__has_include)
#    if __has_include(<boost/unordered/unordered_flat_map.hpp>)
#        include <boost/unordered/unordered_flat_map.hpp>
#        define HUGE_PAGES_HAS_BOOST 1
#    endif
#endif
#if !defined(HUGE_PAGES_HAS_BOOST)
#    define HUGE_PAGES_HAS_BOOST 0
#endif

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <utility>

namespace {

template <class K, class V, template <class> class A>
using map_with = ankerl::unordered_dense::map<K, V, ankerl::unordered_dense::hash<K>, std::equal_to<K>, A<std::pair<K, V>>>;

template <class T>
using std_alloc = std::allocator<T>;
template <class T>
using huge_alloc = ankerl::unordered_dense::huge_page_allocator<T>;
template <class T>
using huge4_alloc = ankerl::unordered_dense::huge_page_allocator<T, (std::size_t{4} << 20U)>;

// The segmented container with a segment sized for huge pages, handed to the map through its
// container slot. A segment is a power of two of elements that fits the byte size, so a 2 MB
// segment of 16 byte pairs is exactly one huge page and a 2 MB segment of 40 byte pairs is 1.28 MB
// and below the allocator's threshold; 16 MB segments bound that rounding at 2 MB per segment.
template <class T>
using seg_std = ankerl::unordered_dense::segmented_vector<T, std::allocator<T>, (std::size_t{2} << 20U)>;
template <class T>
using seg_huge = ankerl::unordered_dense::segmented_vector<T, huge_alloc<T>, (std::size_t{2} << 20U)>;
template <class T>
using seg_huge16 = ankerl::unordered_dense::segmented_vector<T, huge_alloc<T>, (std::size_t{16} << 20U)>;

// A 16 MB segment through the alias's own parameter, which is how a caller writes it now. Same type
// as the `huge_page::` spelling when the allocator is that one, and the allocator stays a parameter.
template <class K, class V, template <class> class A>
using seg16_map = ankerl::unordered_dense::segmented_map<K,
                                                         V,
                                                         ankerl::unordered_dense::hash<K>,
                                                         std::equal_to<K>,
                                                         A<std::pair<K, V>>,
                                                         ankerl::unordered_dense::bucket_type::group,
                                                         (std::size_t{16} << 20U)>;

#if HUGE_PAGES_HAS_BOOST
// boost::unordered_flat_map with the same hash the paired harness gives it (scripts/ab/ab.cpp), and
// the allocator in its slot: one region, so the allocator covers all of it.
template <class K, class V, template <class> class A>
using boost_with =
    boost::unordered_flat_map<K, V, ankerl::unordered_dense::hash<K>, std::equal_to<K>, A<std::pair<K const, V>>>;
#endif

struct result {
    double ns;
    std::size_t ops;
};

template <class Map, std::size_t N>
auto run(std::string const& work) -> result {
    auto ops = std::size_t{};
    auto const t0 = std::chrono::steady_clock::now();
    if (work == "build") {
        workloads::build<Map, N>();
        ops = N;
    } else if (work == "churn") {
        workloads::churn<Map, N>();
        ops = 4 * 4 * N; // four rounds of N, each erase + insert + two finds
    } else if (work == "find") {
        workloads::find_50<Map, N>();
        ops = 100 * N;
    } else if (work == "ie") {
        workloads::insert_erase<Map, N>();
        ops = (N - 1) * 200 * 2;
    } else {
        std::fprintf(stderr, "unknown workload %s\n", work.c_str());
        std::exit(2);
    }
    auto const t1 = std::chrono::steady_clock::now();
    return {static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()), ops};
}

template <class Map>
auto run_size(std::string const& work, std::size_t n) -> result {
    switch (n) {
    case 50000:
        return run<Map, 50000>(work);
    case 200000:
        return run<Map, 200000>(work);
    case 800000:
        return run<Map, 800000>(work);
    case 2000000:
        return run<Map, 2000000>(work);
    case 4000000:
        return run<Map, 4000000>(work);
    default:
        std::fprintf(stderr, "size must be one of 50000 200000 800000 2000000 4000000\n");
        std::exit(2);
    }
}

template <template <class, class, template <class> class> class M, template <class> class A>
auto run_keys(std::string const& keys, std::string const& work, std::size_t n) -> result {
    if (keys == "u64") {
        return run_size<M<std::uint64_t, std::size_t, A>>(work, n);
    }
    if (keys == "str") {
        return run_size<M<std::string, std::size_t, A>>(work, n);
    }
    if (keys == "big") {
        return run_size<M<std::uint64_t, workloads::big_value, A>>(work, n);
    }
    std::fprintf(stderr, "keys must be u64, str or big\n");
    std::exit(2);
}

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc != 5) {
        std::fprintf(
            stderr,
            "usage: %s <std|huge|huge4|seg|seghuge|seghuge16|boost|boosthuge> <u64|str|big> <build|churn|find|ie> <size>\n",
            argv[0]);
        return 2;
    }
    auto const alloc = std::string(argv[1]);
    auto const keys = std::string(argv[2]);
    auto const work = std::string(argv[3]);
    auto const n = static_cast<std::size_t>(std::strtoull(argv[4], nullptr, 10));
    auto r = result{};
    if (alloc == "std") {
        r = run_keys<map_with, std_alloc>(keys, work, n);
    } else if (alloc == "huge") {
        r = run_keys<map_with, huge_alloc>(keys, work, n);
    } else if (alloc == "huge4") {
        r = run_keys<map_with, huge4_alloc>(keys, work, n);
    } else if (alloc == "seg") {
        r = run_keys<map_with, seg_std>(keys, work, n);
    } else if (alloc == "seghuge") {
        r = run_keys<map_with, seg_huge>(keys, work, n);
    } else if (alloc == "seghuge16") {
        r = run_keys<seg16_map, huge_alloc>(keys, work, n);
#if HUGE_PAGES_HAS_BOOST
    } else if (alloc == "boost") {
        r = run_keys<boost_with, std_alloc>(keys, work, n);
    } else if (alloc == "boosthuge") {
        r = run_keys<boost_with, huge_alloc>(keys, work, n);
#endif
    } else {
        std::fprintf(stderr, "allocator must be std, huge, huge4, seg, seghuge, seghuge16, boost or boosthuge\n");
        return 2;
    }
    std::printf("%.3f ns/op %zu ops\n", r.ns / static_cast<double>(r.ops), r.ops);
    return 0;
}
