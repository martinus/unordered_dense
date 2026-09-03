#pragma once

// The workloads of bench_quick_overall_udm, as functions of the map type that return what the
// benchmark checks. Shared with scripts/ab, whose paired A/B replicates the benchmark exactly --
// exactly because there is one copy.

#include <third-party/nanobench.h> // for Rng

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace workloads {

// How long a string key is.
//
// Real string keys are identifiers, field names or paths. Most of them are short, a few are long,
// and no workload has them all the same length. Until 2026-09 every string key here was exactly
// 200 bytes, and that hid two things at once: the length dispatch of the hash was perfectly
// predicted, and every key went on the heap because none of them fit a std::string.
//
// The lengths below run from 8 to 135 bytes, skewed towards short: about a quarter are 16 bytes
// or less, a third are 17 to 48, a quarter are 49 to 96 and a sixth are longer. That covers every
// path the hash has, weighted the way a real workload weighs them, and a quarter of the keys are
// short enough to live inside the std::string.
inline constexpr size_t min_key_len = 8; // room for the whole value, so keys stay distinct
inline constexpr size_t max_key_len = 135;

// What a string key is made of.
//
// Only the length changes what a lookup costs. Neither the hash nor the comparison is faster or
// slower for one byte value than another, so the filler is a fixed string and the value goes in
// the first 8 bytes. Two different values give two different keys, which is what every workload
// here relies on.
inline constexpr std::string_view key_filler =
    "service.name/attribute.count/http.status_code/db.query.duration_ms/net.peer.address.family/"
    "process.runtime.description/log.record.uid/k8s.pod.namespace";

// The key that stands for the value v.
//
// For a string this is a reference to one of a set of buffers prepared once, one per length, and
// making it costs the 8 bytes of the value. Building it with `std::string::assign` instead cost
// far more than the map: `perf` put 17.9% of `findstr` in libstdc++'s `_M_replace` and another
// 15.8% in the `memmove` under it, against 3.3% in the key comparison. What the score compares is
// the map, so a workload must not spend a third of itself making keys.
//
// The buffers are shared, so a key is only valid until the next call for the same length. Every
// workload here uses a key and is done with it.
template <typename K>
struct key_source {
    // Not the value itself. insert_erase and iterate draw their values from a small range, and
    // the hash of a small sequential integer is a multiply, whose top bits walk a lattice: 10000
    // of them in 16384 buckets landed at most one to a bucket, with 39% of buckets empty where a
    // uniform hash leaves 54%. In that table nothing ever collides, so the probe never probes and
    // the shifts never shift -- 82% of erases moved nothing against 58% for the same values as
    // strings -- and the cost of both was invisible. A real integer key is as often an id from
    // somewhere else as a counter, so scramble it: xor-shift between two multiplies is a bijection
    // on 64 bits, which keeps every value distinct and every checksum exactly what it was.
    [[nodiscard]] static auto get(uint64_t v) -> K {
        v *= UINT64_C(0x9E3779B97F4A7C15);
        v ^= v >> 29U;
        v *= UINT64_C(0xBF58476D1CE4E5B9);
        return static_cast<K>(v);
    }
};

template <>
struct key_source<std::string> {
    [[nodiscard]] static auto get(uint64_t v) -> std::string const& {
        static_assert(key_filler.size() >= max_key_len);
        static auto buffers = [] {
            std::vector<std::string> prepared;
            prepared.reserve(max_key_len - min_key_len + 1);
            for (size_t n = min_key_len; n <= max_key_len; ++n) {
                prepared.emplace_back(key_filler.data(), n);
            }
            return prepared;
        }();

        // Square a byte of a mixed value: uniform in, skewed towards short out.
        auto const spread = (v * UINT64_C(0x9E3779B97F4A7C15)) >> 56U;
        auto& key = buffers[static_cast<size_t>((spread * spread) >> 9U)];
        std::memcpy(key.data(), &v, sizeof(v));
        return key;
    }
};

template <typename Map>
[[nodiscard]] auto key_for(uint64_t v) -> decltype(auto) {
    return key_source<typename Map::key_type>::get(v);
}

// the key for a random value in [0, n)
template <typename Map>
[[nodiscard]] auto random_key(ankerl::nanobench::Rng* rng, int n) -> decltype(auto) {
    // we limit ourselves to 32bit n
    auto const limited = (((*rng)() >> 32U) * static_cast<uint64_t>(n)) >> 32U;
    return key_for<Map>(limited);
}

// A mapped type of the size a real one has.
//
// Both maps the score measures hold a `size_t`, and eight bytes of payload hides the property that
// separates a dense map from a flat one. Every cost of a flat map scales with `sizeof(value_type)`,
// because it writes the whole value into a hash-scattered slot; a dense map writes eight bytes
// there and appends the payload to a vector in order. Measured on `build` with the same
// `uint64_t` key, `boost::unordered_flat_map` is 1.22x ahead at a 16 byte value_type and 2.14x
// *behind* at 64 bytes -- the same reason `buildstr` wins and `build64` loses. Nothing in the score
// could see that, so a change that gave it up would have scored the same.
//
// `map<Key, SomeStruct>` is at least as common as `map<Key, size_t>`, and 64 bytes is an ordinary
// struct. It is trivially copyable on purpose: the variable under test is the size of the value,
// and a non-trivial move or an owned allocation would confound it with the heap. The padding is
// value-initialized because a copy of an indeterminate byte is what valgrind is for.
struct big_value {
    // Implicit both ways so that every workload here reads and writes it as it does a size_t, and
    // so the checksums are exactly the ones the small-value maps produce.
    big_value() = default;
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    big_value(size_t v)
        : value(v) {}
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    operator size_t() const {
        return value;
    }

    size_t value{};
    std::array<char, 56> padding{};
};
static_assert(sizeof(big_value) == 64);

struct insert_erase_result {
    size_t erased;
    size_t size;
};

// Random insert & erase, ~10k entries live
template <typename Map>
auto insert_erase() -> insert_erase_result {
    ankerl::nanobench::Rng rng(123);
    size_t erased{};
    Map map;
    for (int n = 1; n < 20000; ++n) {
        for (int i = 0; i < 200; ++i) {
            map[random_key<Map>(&rng, n)];
            erased += map.erase(random_key<Map>(&rng, n));
        }
    }
    return {erased, map.size()};
}

// iterate while adding, then while removing
template <typename Map>
auto iterate() -> size_t {
    size_t const num_elements = 5000;
    ankerl::nanobench::Rng rng(555);
    Map map;
    size_t result = 0;
    for (size_t n = 0; n < num_elements; ++n) {
        map[random_key<Map>(&rng, 1000000)] = n;
        for (auto const& key_val : map) {
            result += key_val.second;
        }
    }

    rng = ankerl::nanobench::Rng(555);
    do {
        map.erase(random_key<Map>(&rng, 1000000));
        for (auto const& key_val : map) {
            result += key_val.second;
        }
    } while (!map.empty());
    return result;
}

// Random finds against a map that grows to ~50k entries while it is searched. Half of the
// lookups find a key that is in the map, chosen at random among those; the other half look for a
// key that cannot be. Which of the two each lookup is, is decided by an rng of its own, so nothing
// here repeats. It used to: the searches replayed the insertion sequence from the same seed, and
// a branch predictor with a long history learned half of the outcomes (0.6 mispredictions per
// lookup where a random sequence costs 1.35). A change that trades instructions for
// mispredictions looked worse here than it was.
template <typename Map>
auto find_50() -> size_t {
    ankerl::nanobench::Rng insert_rng(123123);
    ankerl::nanobench::Rng search_rng(987654321);
    constexpr auto never_inserted = uint64_t{1} << 63U;

    std::vector<uint64_t> inserted;
    inserted.reserve(100000);
    size_t checksum = 0;
    Map map;
    for (size_t i = 0; i < 100000; ++i) {
        // half of the candidates go in; the first one always, so there is something to find
        auto candidate = insert_rng() & ~never_inserted;
        if (inserted.empty() || (insert_rng() & 1U) != 0) {
            if (map.emplace(key_for<Map>(candidate), i).second) {
                inserted.push_back(candidate);
            }
        }

        // search 100 entries in the map
        for (size_t search = 0; search < 100; ++search) {
            auto r = search_rng();
            auto const& key = (r & 1U) != 0 ? key_for<Map>(inserted[((r >> 32U) * inserted.size()) >> 32U])
                                            : key_for<Map>(r | never_inserted);
            auto it = map.find(key);
            if (it != map.end()) {
                checksum += it->second;
            }
        }
    }
    return checksum;
}

// 50k entries, then 10M lookups that all hit (a random key that is there) or all miss (one that
// cannot be): the two ends of what a workload's hit rate can do to the probe.
template <typename Map, bool Hits>
auto find_all() -> size_t {
    ankerl::nanobench::Rng rng(999);
    constexpr auto never_inserted = uint64_t{1} << 63U;
    Map map;
    std::vector<uint64_t> keys;
    keys.reserve(50000);
    while (map.size() < 50000) {
        auto v = rng() & ~never_inserted;
        if (map.emplace(key_for<Map>(v), keys.size()).second) {
            keys.push_back(v);
        }
    }
    size_t checksum = 0;
    auto const num_keys = keys.size();
    for (size_t i = 0; i < 10000000; ++i) {
        auto r = rng();
        auto const& key = key_for<Map>(Hits ? keys[((r >> 32U) * num_keys) >> 32U] : (r | never_inserted));
        auto it = map.find(key);
        if (it != map.end()) {
            checksum += it->second;
        }
    }
    return checksum;
}

// The keys the hash-only workload runs over, built once and shared by every caller.
//
// One set, not one per instantiation: the A/B harness runs two hashes interleaved, and giving each
// of them its own megabytes of keys measures the cache and not the hash. Ten thousand of them is
// more than a branch predictor can remember and still small enough to stay warm.
inline auto hash_keys() -> std::vector<std::string> const& {
    static auto const keys = [] {
        std::vector<std::string> built;
        built.reserve(10000);
        ankerl::nanobench::Rng rng(4711);
        for (size_t i = 0; i < 10000; ++i) {
            built.push_back(key_source<std::string>::get(rng()));
        }
        return built;
    }();
    return keys;
}

// Build a map from empty, which is the half of inserting that insert_erase never shows.
//
// insert_erase draws its keys from a range that grows to 20000, so the map hovers at ~10k entries
// and doubles its bucket array about a dozen times in eight million operations -- growth is a
// rounding error there. It is not one in general: building a map of a million entries against one
// that reserved the room first costs 52% more for uint64_t keys and 31% more for strings, all of
// it rehashing. A map that grew badly would have scored the same as one that grew well.
template <typename Map>
auto build() -> size_t {
    ankerl::nanobench::Rng rng(777);
    Map map;
    for (size_t i = 0; i < 200000; ++i) {
        map[key_for<Map>(rng())] = i;
    }
    return map.size();
}

// Sustained churn at a fixed size, which every other workload here resets before it accumulates.
//
// Backward shift deletion leaves the table as it would have been had the erased element never
// been inserted, so a table that has churned is as good as a fresh one. A design that frees a slot
// without undoing what once probed past it cannot do that: its probe sequences only grow, and it
// repairs them with a rehash. Nothing in the score could tell the two apart, because build and
// insert_erase both keep growing and a growth rehash resets the damage for free.
//
// So this one grows once and then never again: fill to 50000, reserve the room, and from there
// erase one and insert one, holding the size exactly there. Measured this way over two million
// operations, boost::unordered_flat_map's lookups degrade to 1.31x of their fresh cost and snap
// back on a rehash it pays for every third round -- its bucket count never changes -- while this
// map's stay flat within the noise. With string keys both degrade, because what degrades there is
// the heap the key bodies live on rather than the table, and this map churns 1.28x faster than
// boost throughout.
//
// The lookups are interleaved rather than left to the end so that a table which has degraded pays
// for it while it is degraded, which is what a real churning cache does.
template <typename Map>
auto churn() -> size_t {
    constexpr size_t num_elements = 50000;
    constexpr size_t num_rounds = 4;
    constexpr auto never_inserted = uint64_t{1} << 63U;

    ankerl::nanobench::Rng rng(31337);
    Map map;
    map.reserve(num_elements);

    // The keys are a counter, so every insert below brings one the map has never held and every
    // erase names one it holds. live[] is the set of those, and stays exactly num_elements long.
    std::vector<uint64_t> live;
    live.reserve(num_elements);
    uint64_t next = 0;
    while (live.size() < num_elements) {
        map[key_for<Map>(next)] = live.size();
        live.push_back(next);
        ++next;
    }

    size_t checksum = 0;
    for (size_t i = 0; i < num_rounds * num_elements; ++i) {
        auto const slot = static_cast<size_t>(((rng() >> 32U) * live.size()) >> 32U);
        map.erase(key_for<Map>(live[slot]));
        map[key_for<Map>(next)] = i;
        live[slot] = next;
        ++next;

        for (size_t search = 0; search < 2; ++search) {
            auto const r = rng();
            auto const& key =
                (r & 1U) != 0 ? key_for<Map>(live[((r >> 32U) * live.size()) >> 32U]) : key_for<Map>(r | never_inserted);
            auto it = map.find(key);
            if (it != map.end()) {
                checksum += it->second;
            }
        }
    }
    return checksum + map.size();
}

// Hashing alone, over the keys the string workloads use.
//
// A whole lookup is ~167 instructions and only ~59 of them are the hash, and a hash change that
// touches one length range is diluted further by the share of keys in it. This makes the hash the
// whole measurement instead. The keys are built before the loop, so what is timed is hashing and
// not the making of a key.
//
// It resolves a large change and not a small one. A loop this tight with nothing else in it is
// more sensitive to code layout than to a few percent of hashing, and the A/B harness builds two
// of them. See the note in scripts/ab/README.md before believing a small result here.
template <typename Map>
auto hash_strings() -> uint64_t {
    typename Map::hasher const hash{};
    uint64_t checksum = 0; // the hash is 64 bits wide whatever size_t is here
    for (auto const& key : hash_keys()) {
        checksum += hash(key);
    }
    return checksum;
}

} // namespace workloads
