// Two index layouts over identical everything else, so that the window can be measured on its own.
//
// indivi::flat_wmap is the fastest map in the comparison on an integer lookup, and the visible
// difference from its grouped sibling is that it reads sixteen metadata bytes *unaligned starting at
// the home slot* rather than the aligned group the home falls in. Reading the two maps against each
// other cannot separate that from everything else that differs between them -- one byte of metadata
// per slot against two, counters against none, a different author's insert path. This can: the two
// variants below share the value vector, the hash, the fingerprint encoding, the load factor, the
// tombstone policy, the growth policy and the erase, and differ in the home unit and the probe step
// and nothing else.
//
//   VARIANT=0  aligned groups of sixteen, home is a group, probe steps one group
//   VARIANT=1  a sliding window of sixteen, home is a slot, probe steps sixteen slots (flat_wmap)
//   VARIANT=2  ankerl::unordered_dense itself, through the same workload code
//
// 0 against 1 isolates the window. 1 against 2 is the question that follows from it: is a dense map
// built on flat_wmap's structure -- a sliding window, one metadata byte per slot, a uint32 index --
// better than the shipped group index? That comparison is not like-for-like by construction and
// cannot be: a window has no group to hang an overflow counter on, so variant 1 stops a miss on an
// empty slot and has tombstones, where the shipped index stops on a counter and has none. Those
// come as a pair, and the pair is what is being compared.
//
// One variant per binary, because a binary holding both has a code layout that moves when either
// changes -- see CLAUDE.md. Build with scripts/ab/window.sh.
#include <app/name_of_type.h>
#include <bench/workloads.h>

#include <ankerl/unordered_dense.h>

#include <emmintrin.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef VARIANT
#    define VARIANT 0
#endif

namespace {

constexpr std::uint8_t empty_fp = 0;
constexpr std::uint8_t tomb_fp = 1;

// The fingerprint is the low byte of the hash, pre-broadcast into a word, with the two reserved
// values remapped -- boost's table, which both variants use so it cannot bias them.
[[nodiscard]] constexpr auto make_words() -> std::array<std::uint32_t, 256> {
    auto t = std::array<std::uint32_t, 256>{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        t[i] = (i < 2 ? i + 2 : i) * 0x01010101U;
    }
    return t;
}
constexpr auto fp_words = make_words();

template <typename Key, typename Value>
class table {
    using value_type = std::pair<Key, Value>;

    std::vector<value_type> m_values{};
    std::vector<std::uint8_t> m_fp{}; // one per slot; VARIANT 1 mirrors the first 16 past the end
    std::vector<std::uint32_t> m_idx{};
    std::size_t m_mask = 0;   // slots - 1
    std::size_t m_shift = 64; // home = hash >> m_shift
    std::size_t m_used = 0;   // live + tombstones, which is what the load factor counts
    std::size_t m_rehashes = 0;
    mutable std::size_t m_on_empty = 0;
    mutable std::size_t m_on_tomb = 0;
    mutable std::size_t m_probe_windows = 0;
    mutable std::size_t m_probes = 0;
    ankerl::unordered_dense::hash<Key> m_hash{};

    [[nodiscard]] static auto match(__m128i w, std::uint32_t word) -> unsigned {
        return static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpeq_epi8(w, _mm_set1_epi32(static_cast<int>(word)))));
    }
    [[nodiscard]] static auto match_empty(__m128i w) -> unsigned {
        return static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpeq_epi8(w, _mm_setzero_si128())));
    }
    [[nodiscard]] static auto match_avail(__m128i w) -> unsigned {
        auto const e = _mm_cmpeq_epi8(w, _mm_setzero_si128());
        auto const t = _mm_cmpeq_epi8(w, _mm_set1_epi32(0x01010101));
        return static_cast<unsigned>(_mm_movemask_epi8(_mm_or_si128(e, t)));
    }

    [[nodiscard]] auto window(std::size_t at) const -> __m128i {
        return _mm_loadu_si128(reinterpret_cast<__m128i const*>(m_fp.data() + at));
    }
    // The one thing that differs. VARIANT 0's home is a group and its window is that group; its
    // probe walks whole groups. VARIANT 1's home is a slot and its window starts there; its probe
    // walks sixteen slots at a time, so the next window begins where this one ended.
    [[nodiscard]] auto home(std::uint64_t h) const -> std::size_t {
#if VARIANT == 0
        return (h >> m_shift) * 16U;
#else
        return h >> m_shift;
#endif
    }
    [[nodiscard]] auto step(std::size_t at, std::size_t delta) const -> std::size_t {
        return (at + delta * 16U) & m_mask;
    }
    [[nodiscard]] auto slot_of(std::size_t at, unsigned lane) const -> std::size_t {
        return (at + lane) & m_mask;
    }
    void set_fp(std::size_t slot, std::uint8_t v) {
        m_fp[slot] = v;
#if VARIANT == 1
        if (slot < 16U) {
            m_fp[(m_mask + 1U) + slot] = v; // the duplicated first group, so a window may run off the end
        }
#endif
    }

    void grow() {
        ++m_rehashes;
        // never fewer than four groups: a window is sixteen slots wide and both variants read one
        auto slots = m_idx.empty() ? std::size_t{64} : (m_mask + 1U) * 2U;
        while (m_values.size() * 5U >= slots * 4U) {
            slots *= 2U;
        }
        m_mask = slots - 1U;
        m_shift = 64U;
        for (auto s = slots; s > 1U; s >>= 1U) {
            --m_shift;
        }
#if VARIANT == 0
        m_shift += 4U; // the top bits pick a group, not a slot
        m_fp.assign(slots, empty_fp);
#else
        m_fp.assign(slots + 16U, empty_fp);
#endif
        m_idx.assign(slots, 0);
        m_used = 0;
        for (std::uint32_t i = 0; i < m_values.size(); ++i) {
            place(m_hash(m_values[i].first), i);
        }
    }

    void place(std::uint64_t h, std::uint32_t value_idx) {
        auto const word = fp_words[h & 0xFFU];
        auto at = home(h);
        for (std::size_t delta = 1;; ++delta) {
            auto const avail = match_avail(window(at));
            if (avail != 0) {
#if LANE_ROTATE
                // Each key starts its search for a free lane at a hash-derived offset instead of at
                // lane 0. In a grouped design lane 0 is probed first by all sixteen of that group's
                // homes, so it is the lane that gets tombstoned and reused constantly while fresh
                // slots further along are consumed; a per-key start spreads that. Costs a rotate and
                // an and, and nothing at lookup time, because the group is compared whole either way.
                auto const start = static_cast<unsigned>((h >> 8U) & 15U);
                auto const rot = ((avail >> start) | (avail << (16U - start))) & 0xFFFFU;
                auto const lane = (static_cast<unsigned>(__builtin_ctz(rot)) + start) & 15U;
                auto const slot = slot_of(at, lane);
#else
                auto const slot = slot_of(at, static_cast<unsigned>(__builtin_ctz(avail)));
#endif
                if (m_fp[slot] == empty_fp) {
                    ++m_used;
                    ++m_on_empty;
                } else {
                    ++m_on_tomb;
                }
                set_fp(slot, static_cast<std::uint8_t>(word & 0xFFU));
                m_idx[slot] = value_idx;
                return;
            }
            at = step(at, delta);
        }
    }

public:
    [[nodiscard]] auto size() const -> std::size_t {
        return m_values.size();
    }
    [[nodiscard]] auto values() const -> std::vector<value_type> const& {
        return m_values;
    }
    [[nodiscard]] auto probes() const -> std::size_t {
        return m_probes;
    }
    [[nodiscard]] auto probe_windows() const -> std::size_t {
        return m_probe_windows;
    }
    [[nodiscard]] auto on_empty() const -> std::size_t {
        return m_on_empty;
    }
    [[nodiscard]] auto on_tomb() const -> std::size_t {
        return m_on_tomb;
    }
    [[nodiscard]] auto slots() const -> std::size_t {
        return m_mask + 1U;
    }
    [[nodiscard]] auto rehashes() const -> std::size_t {
        return m_rehashes;
    }
    [[nodiscard]] auto footprint() const -> std::size_t {
        return m_values.capacity() * sizeof(value_type) + m_fp.capacity() + m_idx.capacity() * 4U;
    }

    [[nodiscard]] auto find_slot(Key const& key) const -> std::size_t {
        if (m_values.empty()) {
            return SIZE_MAX;
        }
        auto const h = m_hash(key);
        auto const word = fp_words[h & 0xFFU];
        auto at = home(h);
        ++m_probes;
        for (std::size_t delta = 1;; ++delta) {
            ++m_probe_windows;
            auto const w = window(at);
            auto lanes = match(w, word);
            while (lanes != 0) {
                auto const slot = slot_of(at, static_cast<unsigned>(__builtin_ctz(lanes)));
                if (m_values[m_idx[slot]].first == key) {
                    return slot;
                }
                lanes &= lanes - 1U;
            }
            if (match_empty(w) != 0 || delta > m_mask / 16U + 2U) {
                return SIZE_MAX;
            }
            at = step(at, delta);
        }
    }

    [[nodiscard]] auto contains(Key const& key) const -> bool {
        return find_slot(key) != SIZE_MAX;
    }

    auto emplace(Key const& key, Value const& value) -> bool {
        if (auto const s = find_slot(key); s != SIZE_MAX) {
            return false;
        }
        if ((m_used + 1U) * 5U >= (m_mask + 1U) * 4U || m_values.empty()) {
            m_values.emplace_back(key, value);
            grow();
            return true;
        }
        m_values.emplace_back(key, value);
        place(m_hash(key), static_cast<std::uint32_t>(m_values.size() - 1U));
        return true;
    }

    auto erase(Key const& key) -> bool {
        auto const s = find_slot(key);
        if (s == SIZE_MAX) {
            return false;
        }
        auto const gone = m_idx[s];
        set_fp(s, tomb_fp);
        auto const last = static_cast<std::uint32_t>(m_values.size() - 1U);
        if (gone != last) {
            m_values[gone] = std::move(m_values[last]);
            // repoint the moved element: the same second probe both variants pay
            auto const h = m_hash(m_values[gone].first);
            auto const word = fp_words[h & 0xFFU];
            auto at = home(h);
            for (std::size_t delta = 1;; ++delta) {
                auto lanes = match(window(at), word);
                while (lanes != 0) {
                    auto const slot = slot_of(at, static_cast<unsigned>(__builtin_ctz(lanes)));
                    if (m_idx[slot] == last) {
                        m_idx[slot] = gone;
                        goto done;
                    }
                    lanes &= lanes - 1U;
                }
                at = step(at, delta);
            }
        }
    done:
        m_values.pop_back();
        return true;
    }
};

#if VARIANT == 2
// The shipped map, wearing the same interface, so the workloads below cannot tell them apart.
template <typename Key, typename Value>
class udm_table {
    ankerl::unordered_dense::map<Key, Value> m_map{};

public:
    [[nodiscard]] auto size() const -> std::size_t {
        return m_map.size();
    }
    [[nodiscard]] auto contains(Key const& key) const -> bool {
        return m_map.contains(key);
    }
    auto emplace(Key const& key, Value const& value) -> bool {
        return m_map.try_emplace(key, value).second;
    }
    auto erase(Key const& key) -> bool {
        return m_map.erase(key) != 0;
    }
    [[nodiscard]] auto slots() const -> std::size_t {
        return m_map.bucket_count();
    }
    [[nodiscard]] auto rehashes() const -> std::size_t {
        return 0;
    }
    [[nodiscard]] auto on_empty() const -> std::size_t {
        return 0;
    }
    [[nodiscard]] auto on_tomb() const -> std::size_t {
        return 0;
    }
    [[nodiscard]] auto probes() const -> std::size_t {
        return 0;
    }
    [[nodiscard]] auto probe_windows() const -> std::size_t {
        return 0;
    }
    [[nodiscard]] auto footprint() const -> std::size_t {
        // 88 bytes of merged block per sixteen slots, plus the value vector
        return m_map.values().capacity() * sizeof(std::pair<Key, Value>) + m_map.bucket_count() / 16U * 88U;
    }
};
#endif

} // namespace

// ---------------------------------------------------------------------------------------------
// workloads. Same shapes as the scored suite, and the same key generator, so a number here is
// comparable with one from maps_one.sh.

namespace {

using clock_type = std::chrono::steady_clock;

template <typename Fn>
auto timed(std::size_t ops, Fn&& fn) -> double {
    auto const t0 = clock_type::now();
    fn();
    auto const t1 = clock_type::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(ops);
}

template <typename Key>
auto pool(std::size_t n, std::uint64_t seed) -> std::vector<Key> {
    auto rng = ankerl::nanobench::Rng(seed);
    auto v = std::vector<Key>();
    v.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        v.push_back(workloads::key_source<Key>::get(rng()));
    }
    return v;
}

// A cross-check against std::unordered_map over a mixed stream, so that a fast wrong answer is
// caught before anything is timed.
#if VARIANT == 2
template <typename K, typename V>
using map_type = udm_table<K, V>;
#else
template <typename K, typename V>
using map_type = table<K, V>;
#endif

template <typename Key>
void check(std::size_t n) {
    auto t = map_type<Key, std::size_t>();
    auto ref = std::unordered_map<Key, std::size_t>();
    auto rng = ankerl::nanobench::Rng(99);
    auto keys = pool<Key>(n, 12345);
    for (std::size_t i = 0; i < n * 8U; ++i) {
        auto const& k = keys[(rng() >> 32U) * keys.size() >> 32U];
        switch (rng() % 3U) {
        case 0: {
            auto const a = t.emplace(k, i);
            auto const b = ref.emplace(k, i).second;
            if (a != b) {
                std::puts("MISMATCH emplace");
                std::exit(1);
            }
            break;
        }
        case 1: {
            if (t.erase(k) != (ref.erase(k) != 0)) {
                std::puts("MISMATCH erase");
                std::exit(1);
            }
            break;
        }
        default:
            if (t.contains(k) != (ref.count(k) != 0)) {
                std::puts("MISMATCH contains");
                std::exit(1);
            }
        }
        if (t.size() != ref.size()) {
            std::puts("MISMATCH size");
            std::exit(1);
        }
    }
    for (auto const& [k, v] : ref) {
        if (!t.contains(k)) {
            std::puts("MISMATCH content");
            std::exit(1);
        }
    }
    std::printf("check ok, %zu entries\n", t.size());
}

template <typename Key>
void run(std::string const& what, std::size_t n, std::size_t reps) {
    auto present = pool<Key>(n, 1);
    auto absent = pool<Key>(n, 2);
    auto spare = pool<Key>(n, 3);
    auto rng = ankerl::nanobench::Rng(7);
    auto acc = std::size_t{0};

    if (what == "build") {
        auto ns = 0.0;
        for (std::size_t r = 0; r < reps; ++r) {
            auto t = map_type<Key, std::size_t>();
            ns += timed(n, [&] {
                for (std::size_t i = 0; i < n; ++i) {
                    t.emplace(present[i], i);
                }
            });
            acc += t.size();
        }
        std::printf("%-6s %8.2f ns/op\n", what.c_str(), ns / static_cast<double>(reps));
    } else {
        auto t = map_type<Key, std::size_t>();
        for (std::size_t i = 0; i < n; ++i) {
            t.emplace(present[i], i);
        }
        if (what == "memory") {
            std::printf(
                "%-6s %8.2f bytes/entry\n", what.c_str(), static_cast<double>(t.footprint()) / static_cast<double>(t.size()));
            return;
        }
        if (what == "churn") {
            // Erase a live key and insert one the map does not hold, at an exactly constant size.
            // `live` tracks what is in the map so every erase succeeds -- erasing from a fixed pool
            // drains it and the loop quietly stops inserting -- and the erased key is swapped back
            // into the spare pool so that no key is constructed inside the timed region.
            auto live = present;
            std::size_t next = 0;
            auto ns = timed(reps, [&] {
                for (std::size_t i = 0; i < reps; ++i) {
                    auto const at = static_cast<std::size_t>((rng() >> 32U) * live.size() >> 32U);
                    t.erase(live[at]);
                    t.emplace(spare[next], i);
                    std::swap(live[at], spare[next]);
                    next = (next + 1U) % spare.size();
                    acc += t.size();
                }
            });
            std::printf("%-6s %8.2f ns/op  rehashes=%zu slots=%zu\n", what.c_str(), ns, t.rehashes(), t.slots());
            std::printf("       placements: %zu onto an empty slot, %zu onto a tombstone (%.1f%% recycled)\n",
                        t.on_empty(),
                        t.on_tomb(),
                        100.0 * static_cast<double>(t.on_tomb()) / static_cast<double>(t.on_empty() + t.on_tomb()));
        } else {
            auto const& keys = (what == "hit") ? present : absent;
            auto const p0 = t.probes();
            auto const w0 = t.probe_windows();
            auto ns = timed(reps, [&] {
                for (std::size_t i = 0; i < reps; ++i) {
                    acc += t.contains(keys[(rng() >> 32U) * keys.size() >> 32U]) ? 1U : 0U;
                }
            });
            std::printf("%-6s %8.2f ns/op  windows visited per lookup %.4f\n",
                        what.c_str(),
                        ns,
                        static_cast<double>(t.probe_windows() - w0) / static_cast<double>(t.probes() - p0));
        }
    }
    ankerl::nanobench::doNotOptimizeAway(acc);
}

} // namespace

int main(int argc, char** argv) {
    workloads::tame_allocator();
    auto const what = std::string(argc > 1 ? argv[1] : "hit");
    auto const n = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 200000;
    auto const reps = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 3000000;
#ifdef WINDOW_STR
    using key_type = std::string;
#else
    using key_type = std::uint64_t;
#endif
    if (what == "check") {
        check<key_type>(n);
        return 0;
    }
    run<key_type>(what, n, reps);
}
