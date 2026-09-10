// Bytes held against wall-clock time while a map is filled, which is the picture of how a
// container grows rather than of what it ends up costing.
//
// Three things make this honest, and the chart it draws is wrong without any one of them.
//
// *Every* allocation is counted, by replacing global operator new and delete rather than by handing
// the map an allocator. A container allocator sees only what the container asks for through it: not
// what a value's own constructor allocates, not what a segmented container's book-keeping costs,
// and -- the reason it matters here -- nothing at all of a map whose allocator is not a template
// parameter this harness can reach. Replacing new counts what the process actually holds.
//
// The malloc overhead is counted too. A request for 24 bytes does not cost 24: glibc rounds every
// chunk up to a multiple of 16 with a minimum of 32 and keeps an 8 byte header, so
// `malloc_usable_size` plus that header is what the heap really gave away. Asking the allocator
// after the fact is the only exact way to know -- a formula has to guess the allocator's version,
// and a size prefix of one's own changes the very chunk it is trying to measure.
//
// And every change is stamped with `std::chrono::steady_clock` as it happens, so the x axis is the
// time the growth actually took. A steady clock rather than a system one: this is an elapsed
// duration, and a system clock may be stepped underneath it.
//
//   scripts/ab/alloc_timeline.sh
//
// writes one CSV per map into doc/ and draws doc/allocated_memory.png from them.
#include <ankerl/unordered_dense.h>

#if __has_include(<boost/unordered/unordered_flat_map.hpp>)
#    include <boost/unordered/unordered_flat_map.hpp>
#    define UDM_HAVE_BOOST 1 // NOLINT(cppcoreguidelines-macro-usage)
#endif
#if __has_include(<absl/container/flat_hash_map.h>)
#    include <absl/container/flat_hash_map.h>
#    define UDM_HAVE_ABSL 1 // NOLINT(cppcoreguidelines-macro-usage)
#endif

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#if __has_include(<malloc.h>)
#    include <malloc.h> // malloc_usable_size
#elif defined(__APPLE__)
#    include <malloc/malloc.h> // malloc_size
#endif

namespace {

// What one allocation really takes out of the heap. Everything else in this file is book-keeping
// around this one question, and it is the question a size-tracking allocator cannot answer.
auto heap_footprint(void* p) -> std::size_t {
#if defined(__GLIBC__) || defined(__linux__)
    // glibc hands back a chunk of `usable + 8`: the size field it keeps in front of the pointer.
    // The rounding to 16 with a 32 byte floor is already inside the usable size.
    return malloc_usable_size(p) + sizeof(std::size_t);
#elif defined(__APPLE__)
    return malloc_size(p);
#elif defined(_MSC_VER)
    return _msize(p);
#else
#    error "no way to ask this allocator how big a block really is"
#endif
}

// One row of the chart: seconds since the fill started, and the bytes held once this change landed.
struct event {
    double seconds;
    std::size_t bytes;
};

// Records every change while it is armed. It cannot use a std::vector, or any other container:
// operator new is replaced below and would recurse straight back into here, so the buffer is grown
// with plain realloc and is deliberately invisible to the count it keeps.
class timeline {
    event* m_events = nullptr;
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;
    std::chrono::steady_clock::time_point m_start{};
    std::size_t m_live = 0;
    std::size_t m_peak = 0;

public:
    timeline() = default;
    timeline(timeline const&) = delete;
    auto operator=(timeline const&) -> timeline& = delete;

    ~timeline() {
        std::free(m_events);
    }

    void restart() {
        m_size = 0;
        m_live = 0;
        m_peak = 0;
        m_start = std::chrono::steady_clock::now();
    }

    void note(std::size_t bytes, bool freed) {
        if (freed) {
            m_live -= bytes;
        } else {
            m_live += bytes;
            if (m_live > m_peak) {
                m_peak = m_live;
            }
        }
        if (m_size == m_capacity) {
            auto const capacity = m_capacity == 0 ? std::size_t{1024} : m_capacity * 2;
            auto* grown = static_cast<event*>(std::realloc(m_events, capacity * sizeof(event)));
            if (grown == nullptr) {
                std::abort();
            }
            m_events = grown;
            m_capacity = capacity;
        }
        // The clock is read here, when the change happens, rather than being reconstructed later
        // from an allocation count: what the chart is about is how long the growth took.
        auto const elapsed = std::chrono::steady_clock::now() - m_start;
        m_events[m_size++] = event{std::chrono::duration<double>(elapsed).count(), m_live};
    }

    [[nodiscard]] auto peak() const -> std::size_t {
        return m_peak;
    }

    [[nodiscard]] auto live() const -> std::size_t {
        return m_live;
    }

    [[nodiscard]] auto count() const -> std::size_t {
        return m_size;
    }

    [[nodiscard]] auto seconds() const -> double {
        return m_size == 0 ? 0.0 : m_events[m_size - 1].seconds;
    }

    void save(char const* path) const {
        auto* out = std::fopen(path, "w");
        if (out == nullptr) {
            std::fprintf(stderr, "cannot write %s\n", path);
            std::exit(1);
        }
        std::fprintf(out, "# seconds,bytes\n");
        for (std::size_t i = 0; i < m_size; ++i) {
            std::fprintf(out, "%.9f,%zu\n", m_events[i].seconds, m_events[i].bytes);
        }
        std::fclose(out);
    }
};

// Null except while a map is being filled, so the rest of the program -- reading arguments, writing
// the CSVs, tearing a map down again -- is not charged to any map.
timeline* g_recording = nullptr;

auto tracked_malloc(std::size_t n) -> void* {
    auto* p = std::malloc(n == 0 ? 1 : n);
    if (p != nullptr && g_recording != nullptr) {
        g_recording->note(heap_footprint(p), false);
    }
    return p;
}

void tracked_free(void* p) {
    if (p == nullptr) {
        return;
    }
    // Before the free, because afterwards the block is not ours to ask about.
    if (g_recording != nullptr) {
        g_recording->note(heap_footprint(p), true);
    }
    std::free(p);
}

auto tracked_aligned_malloc(std::size_t n, std::size_t alignment) -> void* {
    // aligned_alloc wants a size that is a multiple of the alignment.
    auto const rounded = (n + alignment - 1) & ~(alignment - 1);
    auto* p = std::aligned_alloc(alignment, rounded == 0 ? alignment : rounded);
    if (p != nullptr && g_recording != nullptr) {
        g_recording->note(heap_footprint(p), false);
    }
    return p;
}

struct rng {
    std::uint64_t s = 0x853c49e6748fea9bULL;

    auto operator()() -> std::uint64_t {
        s ^= s << 13U;
        s ^= s >> 7U;
        s ^= s << 17U;
        return s;
    }
};

constexpr auto num_elements = std::size_t{10'000'000};

template <typename Map>
void measure(char const* name, char const* path) {
    auto t = timeline();
    {
        auto map = Map();
        auto r = rng();
        t.restart();
        g_recording = &t;
        for (std::size_t i = 0; i < num_elements; ++i) {
            map[r()] = i;
        }
        g_recording = nullptr;
        if (map.size() != num_elements) {
            std::fprintf(stderr, "%s: %zu elements, expected %zu\n", name, map.size(), num_elements);
            std::exit(1);
        }
        // The map is still alive here on purpose: the last row of the chart is the steady state,
        // not the cliff of a destructor.
        t.save(path);
    }
    std::printf("%-46s %7.1f MB steady  %7.1f MB peak  %6.3f s  %zu allocations\n",
                name,
                static_cast<double>(t.live()) / 1e6,
                static_cast<double>(t.peak()) / 1e6,
                t.seconds(),
                t.count());
    std::fflush(stdout);
}

using hash_t = ankerl::unordered_dense::hash<std::uint64_t>;
using eq_t = std::equal_to<std::uint64_t>;

} // namespace

// The replacements themselves. Every form the standard defines, because a form left out is served
// by the default implementation and its partner here would then free a block malloc never gave out.
auto operator new(std::size_t n) -> void* {
    auto* p = tracked_malloc(n);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

auto operator new[](std::size_t n) -> void* {
    return ::operator new(n);
}

auto operator new(std::size_t n, std::nothrow_t const& /*tag*/) noexcept -> void* {
    return tracked_malloc(n);
}

auto operator new[](std::size_t n, std::nothrow_t const& /*tag*/) noexcept -> void* {
    return tracked_malloc(n);
}

auto operator new(std::size_t n, std::align_val_t a) -> void* {
    auto* p = tracked_aligned_malloc(n, static_cast<std::size_t>(a));
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}

auto operator new[](std::size_t n, std::align_val_t a) -> void* {
    return ::operator new(n, a);
}

auto operator new(std::size_t n, std::align_val_t a, std::nothrow_t const& /*tag*/) noexcept -> void* {
    return tracked_aligned_malloc(n, static_cast<std::size_t>(a));
}

auto operator new[](std::size_t n, std::align_val_t a, std::nothrow_t const& /*tag*/) noexcept -> void* {
    return tracked_aligned_malloc(n, static_cast<std::size_t>(a));
}

void operator delete(void* p) noexcept {
    tracked_free(p);
}

void operator delete[](void* p) noexcept {
    tracked_free(p);
}

void operator delete(void* p, std::size_t /*n*/) noexcept {
    tracked_free(p);
}

void operator delete[](void* p, std::size_t /*n*/) noexcept {
    tracked_free(p);
}

void operator delete(void* p, std::nothrow_t const& /*tag*/) noexcept {
    tracked_free(p);
}

void operator delete[](void* p, std::nothrow_t const& /*tag*/) noexcept {
    tracked_free(p);
}

void operator delete(void* p, std::align_val_t /*a*/) noexcept {
    tracked_free(p);
}

void operator delete[](void* p, std::align_val_t /*a*/) noexcept {
    tracked_free(p);
}

void operator delete(void* p, std::size_t /*n*/, std::align_val_t /*a*/) noexcept {
    tracked_free(p);
}

void operator delete[](void* p, std::size_t /*n*/, std::align_val_t /*a*/) noexcept {
    tracked_free(p);
}

void operator delete(void* p, std::align_val_t /*a*/, std::nothrow_t const& /*tag*/) noexcept {
    tracked_free(p);
}

void operator delete[](void* p, std::align_val_t /*a*/, std::nothrow_t const& /*tag*/) noexcept {
    tracked_free(p);
}

auto main(int argc, char** argv) -> int {
    auto const* dir = argc > 1 ? argv[1] : "doc";
    auto path = [&](char const* name) {
        static char buffer[1024];
        std::snprintf(buffer, sizeof(buffer), "%s/allocated_memory_%s.csv", dir, name);
        return buffer;
    };

    std::printf("filling each map with %zu uint64_t -> uint64_t pairs\n", num_elements);

    measure<ankerl::unordered_dense::map<std::uint64_t, std::uint64_t, hash_t, eq_t>>(
        "ankerl::unordered_dense::map", path("map"));
    measure<ankerl::unordered_dense::segmented_map<std::uint64_t, std::uint64_t, hash_t, eq_t>>(
        "ankerl::unordered_dense::segmented_map", path("segmented_map"));
#ifdef UDM_HAVE_BOOST
    measure<boost::unordered_flat_map<std::uint64_t, std::uint64_t, hash_t, eq_t>>("boost::unordered_flat_map",
                                                                                  path("boost_flat_map"));
#endif
#ifdef UDM_HAVE_ABSL
    measure<absl::flat_hash_map<std::uint64_t, std::uint64_t, hash_t, eq_t>>("absl::flat_hash_map",
                                                                            path("absl_flat_hash_map"));
#endif
    return 0;
}
