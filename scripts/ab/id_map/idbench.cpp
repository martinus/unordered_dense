// id_map candidates against std::unordered_map, this repository's map and a vector indexed by ID (#379).
//
// Three ID patterns, each a formula of the entry number i so that a lookup computes its key instead
// of reading it from an array (which would measure that array's misses):
//   dense   id = i                               Redpanda's raft groups, OSRM's nodes
//   ifc     id = floor(1.75 i)                   steps of 1 and 2, as web-ifc's express IDs
//   sparse  id = 16 i + (hash(i) & 15)           1/16 of the range, scattered
// Per cell: build (insert ids in ascending order), then a million lookups of random entries, all hits,
// independent of each other (throughput, not latency). Median of five rounds after one warm-up; the
// container is destroyed outside the clock; memory is malloc's bytes in use after the build.
//
//   clang++ -O3 -DNDEBUG -std=c++17 -I<repo>/include idbench.cpp && taskset -c 2 ./a.out
#include "id_map.h"

#include <ankerl/unordered_dense.h>

#include <malloc.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct value {
    std::uint32_t a;
    std::uint32_t b;
};

// what a caller writes today: a vector indexed by ID, a slot per ID, growing by doubling
template <class K, class T>
class vec_map {
    std::vector<std::optional<T>> m_v;
    std::size_t m_size = 0;

public:
    explicit vec_map(std::size_t = 0) {}
    auto emplace(K id, T const& v) -> bool {
        if (id >= m_v.size()) {
            m_v.resize(std::max<std::size_t>(id + 1, m_v.size() * 2));
        }
        if (m_v[id]) {
            return false;
        }
        m_v[id] = v;
        ++m_size;
        return true;
    }
    auto find_ptr(K id) const -> T const* {
        return id < m_v.size() && m_v[id] ? &*m_v[id] : nullptr;
    }
    auto erase(K id) -> std::size_t {
        if (id >= m_v.size() || !m_v[id]) {
            return 0;
        }
        m_v[id].reset();
        --m_size;
        return 1;
    }
    auto size() const -> std::size_t {
        return m_size;
    }
};

// one interface for the harness: insert and a pointer lookup
template <class M>
auto lookup(M const& m, std::uint64_t k) -> value const* {
    auto it = m.find(static_cast<typename M::key_type>(k));
    return it == m.end() ? nullptr : &it->second;
}
template <class K, class T, unsigned B, bool P, class U>
auto lookup(idm::id_map<K, T, B, P, U> const& m, std::uint64_t k) -> T const* {
    return m.find_ptr(k);
}
template <class K, class T>
auto lookup(vec_map<K, T> const& m, std::uint64_t k) -> T const* {
    return m.find_ptr(k);
}
template <class K, class T, unsigned B>
auto lookup(idm::paged_map<K, T, B> const& m, std::uint64_t k) -> T const* {
    return m.find_ptr(k);
}
template <class K, class T, idm::layout L>
auto lookup(idm::packed_map<K, T, L> const& m, std::uint64_t k) -> T const* {
    return m.find_ptr(k);
}

template <class M>
void insert(M& m, std::uint64_t k, value v) {
    m.emplace(static_cast<std::uint32_t>(k), v);
}

struct rng {
    std::uint64_t s;
    auto operator()() -> std::uint64_t {
        s += 0x9E3779B97F4A7C15ULL;
        auto z = s;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }
};

enum class pattern { dense, ifc, sparse, scatter };
auto id_of(pattern p, std::uint64_t i) -> std::uint64_t {
    switch (p) {
    case pattern::dense:
        return i;
    case pattern::ifc:
        return (i * 7) / 4;
    case pattern::sparse:
        return i * 16 + (((i * 0x9E3779B97F4A7C15ULL) >> 60U) & 15U);
    case pattern::scatter: // one ID per 4096: every 4096-ID page holds a single entry
        return i * 4096 + (((i * 0x9E3779B97F4A7C15ULL) >> 52U) & 4095U);
    }
    return 0;
}

volatile std::uint64_t sink = 0;

auto now() -> double {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
auto heap_bytes() -> std::size_t {
    auto mi = mallinfo2();
    return mi.uordblks + mi.hblkhd;
}

template <class M>
void cell(char const* name, pattern p, char const* pname, std::size_t n) {
    std::vector<double> build;
    std::vector<double> find;
    std::size_t bytes = 0;
    for (int round = 0; round < 6; ++round) {
        auto const h0 = heap_bytes();
        auto* m = new M();
        auto const t0 = now();
        for (std::uint64_t i = 0; i < n; ++i) {
            insert(*m, id_of(p, i), value{static_cast<std::uint32_t>(i), 1});
        }
        auto const t1 = now();
        auto const h1 = heap_bytes();
        rng r{static_cast<std::uint64_t>(round) * 977};
        std::uint64_t sum = 0;
        auto const t2 = now();
        for (int q = 0; q < 1000000; ++q) {
            auto const i = ((r() >> 32U) * n) >> 32U;
            // use the miss: dereferencing unchecked lets the compiler assume a hit and delete the
            // presence test, which clang did for id_map's bitmap read (9 of 44 cycles per lookup)
            auto const* v = lookup(*m, id_of(p, i));
            sum += v != nullptr ? v->a : 7;
        }
        auto const t3 = now();
        sink = sink + sum;
        delete m;
        if (round > 0) {
            build.push_back((t1 - t0) / static_cast<double>(n) * 1e9);
            find.push_back((t3 - t2) * 1e3);
            bytes = h1 - h0;
        }
    }
    std::sort(build.begin(), build.end());
    std::sort(find.begin(), find.end());
    std::printf("%-8s %-10zu %-14s build %6.2f ns/id  find %6.2f ns/op  %6.2f B/entry\n",
                pname,
                n,
                name,
                build[2],
                find[2],
                static_cast<double>(bytes) / static_cast<double>(n));
}

// Churn with IDs that move upward, as a system that allocates IDs in ascending order: start with
// 0..n-1, then 10 n times erase a random live ID and insert the next new ID, so the table stays at n
// entries while its IDs drift up and old pages empty out. The live IDs are kept in an array, whose
// read is the same cache miss for every container. Reports ns per erase+insert (median of five after
// a warm-up) and malloc's bytes per live entry at the end, the array not counted.
template <class M>
void churn_cell(char const* name, std::size_t n) {
    std::vector<double> t;
    std::size_t bytes = 0;
    for (int round = 0; round < 6; ++round) {
        std::vector<std::uint32_t> ids(n);
        for (std::size_t i = 0; i < n; ++i) {
            ids[i] = static_cast<std::uint32_t>(i);
        }
        auto const h0 = heap_bytes();
        auto* m = new M();
        for (std::uint64_t i = 0; i < n; ++i) {
            insert(*m, i, value{static_cast<std::uint32_t>(i), 1});
        }
        rng r{static_cast<std::uint64_t>(round) * 31 + 7};
        std::uint64_t hi = n;
        auto const ops = 10 * n;
        auto const t0 = now();
        for (std::size_t q = 0; q < ops; ++q) {
            auto const j = ((r() >> 32U) * n) >> 32U;
            m->erase(ids[j]);
            ids[j] = static_cast<std::uint32_t>(hi);
            insert(*m, hi, value{static_cast<std::uint32_t>(hi), 1});
            ++hi;
        }
        auto const t1 = now();
        auto const h1 = heap_bytes();
        if (m->size() != n) {
            std::printf("churn %s: size %zu, expected %zu\n", name, m->size(), n);
        }
        delete m;
        if (round > 0) {
            t.push_back((t1 - t0) / static_cast<double>(ops) * 1e9);
            bytes = h1 - h0;
        }
    }
    std::sort(t.begin(), t.end());
    std::printf("churn    %-10zu %-14s op %6.2f ns  %8.2f B/live entry\n",
                n,
                name,
                t[2],
                static_cast<double>(bytes) / static_cast<double>(n));
}

template <class K>
void churn_all(std::size_t n) {
    churn_cell<std::unordered_map<K, value>>("std", n);
    churn_cell<ankerl::unordered_dense::map<K, value>>("map", n);
    churn_cell<vec_map<K, value>>("vector", n);
    churn_cell<idm::id_map<K, value>>("id_map", n);
    churn_cell<idm::id_map<K, value, 12, true>>("id_map_pairs", n);
}

template <class K>
void all(pattern p, char const* pname, std::size_t n) {
    cell<std::unordered_map<K, value>>("std", p, pname, n);
    cell<ankerl::unordered_dense::map<K, value>>("map", p, pname, n);
    cell<vec_map<K, value>>("vector", p, pname, n);
    cell<idm::paged_map<K, value, 12>>("paged12", p, pname, n);
    cell<idm::packed_map<K, value, idm::layout::packed>>("packed", p, pname, n);
    cell<idm::id_map<K, value>>("id_map", p, pname, n);
    cell<idm::id_map<K, value, 12, true>>("id_map_pairs", p, pname, n);
}

// id_map's knobs (#379). The first round (one allocation per direct page, 4x growth, back to packed
// at a quarter) is in data/tune.txt, the tagged directory entry in data/tag.txt; this round measures
// the page metadata: direct pages without `before` (slim) and pages of 1024 IDs instead of 4096.
template <class K>
void tune_all(pattern p, char const* pname, std::size_t n) {
    cell<ankerl::unordered_dense::map<K, value>>("map", p, pname, n);
    cell<idm::id_map<K, value, 12, false, idm::tune<>>>("default", p, pname, n);
    cell<idm::id_map<K, value, 12, false, idm::tune<false, 1, 8, false, true>>>("slim", p, pname, n);
    cell<idm::id_map<K, value, 10, false, idm::tune<>>>("p10", p, pname, n);
    cell<idm::id_map<K, value, 10, false, idm::tune<false, 1, 8, false, true>>>("p10_slim", p, pname, n);
}
template <class K>
void tune_churn(std::size_t n) {
    churn_cell<ankerl::unordered_dense::map<K, value>>("map", n);
    churn_cell<idm::id_map<K, value, 12, false, idm::tune<>>>("default", n);
    churn_cell<idm::id_map<K, value, 12, false, idm::tune<false, 1, 8, false, true>>>("slim", n);
    churn_cell<idm::id_map<K, value, 10, false, idm::tune<>>>("p10", n);
    churn_cell<idm::id_map<K, value, 10, false, idm::tune<false, 1, 8, false, true>>>("p10_slim", n);
}
// the directory's worst case: 1000 dense IDs and one ID near 4 billion
template <class M>
void stray_cell(char const* name) {
    auto const h0 = heap_bytes();
    auto* m = new M();
    for (std::uint32_t i = 0; i < 1000; ++i) {
        m->emplace(i, value{i, 1});
    }
    m->emplace(4000000000U, value{0, 1});
    auto const h1 = heap_bytes();
    std::printf("stray    1001       %-14s %10.2f MB\n", name, static_cast<double>(h1 - h0) / 1e6);
    delete m;
}

} // namespace

auto main(int argc, char** argv) -> int {
    mallopt(M_MMAP_THRESHOLD, 1 << 30);
    mallopt(M_TRIM_THRESHOLD, 1 << 30);
    auto const only = argc > 1 ? std::string(argv[1]) : std::string();
    auto want = [&](char const* p) {
        return only.empty() || only == p;
    };
    if (want("dense")) {
        for (std::size_t n : {1000, 16000, 100000, 1000000, 3500000})
            all<std::uint32_t>(pattern::dense, "dense", n);
    }
    if (want("ifc")) {
        for (std::size_t n : {16000, 100000, 1000000, 3500000})
            all<std::uint32_t>(pattern::ifc, "ifc", n);
    }
    if (only == "tune") {
        for (std::size_t n : {1000, 100000, 1000000, 3500000})
            tune_all<std::uint32_t>(pattern::dense, "dense", n);
        for (std::size_t n : {100000, 1000000, 3500000})
            tune_all<std::uint32_t>(pattern::ifc, "ifc", n);
        for (std::size_t n : {100000, 1000000})
            tune_all<std::uint32_t>(pattern::sparse, "sparse", n);
        for (std::size_t n : {100000, 1000000})
            tune_churn<std::uint32_t>(n);
        for (std::size_t n : {16000, 100000, 1000000})
            tune_all<std::uint32_t>(pattern::scatter, "scatter", n);
        stray_cell<ankerl::unordered_dense::map<std::uint32_t, value>>("map");
        stray_cell<idm::id_map<std::uint32_t, value, 12, false, idm::tune<>>>("default");
        stray_cell<idm::id_map<std::uint32_t, value, 10, false, idm::tune<>>>("p10");
        return 0;
    }
    if (want("churn")) {
        for (std::size_t n : {16000, 100000, 1000000})
            churn_all<std::uint32_t>(n);
    }
    if (want("sparse")) {
        for (std::size_t n : {16000, 100000, 1000000})
            all<std::uint32_t>(pattern::sparse, "sparse", n);
    }
}
