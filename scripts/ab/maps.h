#pragma once

// Every index structure in the blog post, behind one interface, with the keys and workloads the
// scored benchmark uses. Shared by scripts/ab/maps.cpp (all of them interleaved in one process,
// which is how a ratio is measured) and scripts/ab/maps_one.cpp (one per binary, which is how a
// hardware counter is attributed).
//
// What this measures is the *index* -- the metadata a map reads before it touches a key -- so
// every map is handed the same hash (this project's wyhash), and the two maps whose own hash a
// caller actually gets by default (boost and abseil) appear a second time with it as a control.
// That control is not cosmetic: for string keys it reverses the ranking.
//
// Built by scripts/ab/maps.sh, which is where the include paths and the 4.11.0 rename live.

#include <ankerl/unordered_dense.h> // the group index, 5.0
#include <base411.h>                // 4.11.0, renamed into namespace udmbase by maps.sh
#if __has_include(<sys/mman.h>)
#    define UDM_HAVE_HUGE 1
#    include <ankerl/huge_page_allocator.h>
#endif

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_node_map.hpp>

#include <bench/workloads.h> // key_for, tame_allocator: the scored benchmark's own keys
#include <third-party/nanobench.h>

#if __has_include(<absl/container/flat_hash_map.h>) && !defined(UDM_NO_ABSL)
#    define UDM_HAVE_ABSL 1
#    include <absl/container/flat_hash_map.h>
#    include <absl/container/node_hash_map.h>
#endif
#if __has_include(<folly/container/F14Map.h>)
#    define UDM_HAVE_F14 1
#    include <folly/container/F14Map.h>
#endif
#if __has_include(<emhash/hash_table8.hpp>)
#    define UDM_HAVE_EMHASH 1
#    include <emhash/hash_table8.hpp>
#    include <emilib/emihmap1.hpp>
#endif
#if __has_include(<indivi/flat_umap.h>)
#    define UDM_HAVE_INDIVI 1
#    include <indivi/flat_umap.h>
#    include <indivi/flat_wmap.h>
#endif
#if __has_include(<ihtab.hpp>)
#    define UDM_HAVE_IHTAB 1
#    include <ihtab.hpp>
#endif

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__GLIBC__)
#    include <malloc.h>
#endif

namespace udm_maps {
// Whether every map is handed this project's hash or its own.
//
// The default here is the same hash for all of them, because what maps.cpp asks is which *index* is
// faster and a hash is not an index. `-DUDM_DEFAULT_HASH` asks the other question -- what a caller
// gets by typing the type name, which is the library's own hash and the configuration a README
// graph is about. For a string key the two questions have different answers, which is why the
// same-hash runs carry `boost-own` and `absl-own` as controls.
#if defined(UDM_DEFAULT_HASH)
#    define UDM_HASH(Key)
// Spelled out for the one adapter whose hash is not its last argument, where leaving the slot empty
// would shift everything after it along.
#    define UDM_HASH_HERE(Key) ankerl::unordered_dense::hash<Key>
#else
#    define UDM_HASH(Key) , same_hash<Key>
#    define UDM_HASH_HERE(Key) same_hash<Key>
#endif

// The one hash every map is given, so that what differs between them is the index.
//
// The two typedefs are how a map is told the hash is already well mixed and needs no further
// mixing of its own -- and each library spells it differently. Without folly's, F14 puts an extra
// CRC32 step in front of every lookup and is no longer being handed the same hash as the rest;
// boost and this library both read `is_avalanching`, and abseil and the others do no mixing
// either way.
template <typename Key>
struct same_hash : ankerl::unordered_dense::hash<Key> {
    using is_avalanching = void;                 // ankerl, boost
    using folly_is_avalanching = std::true_type; // folly
};
} // namespace udm_maps
using udm_maps::same_hash;

#if __has_include(<verstable.h>)
#    define UDM_HAVE_VERSTABLE 1
// Verstable is a C99 macro template. It wants a plain function; hand it the same wyhash.
static inline auto udm_hash_u64(std::uint64_t k) -> std::uint64_t {
    return same_hash<std::uint64_t>{}(k);
}
#    define NAME vtab
#    define KEY_TY std::uint64_t
#    define VAL_TY std::size_t
#    define HASH_FN udm_hash_u64
#    define CMPR_FN vt_cmpr_integer
#    include <verstable.h>
#endif

namespace udm_maps {

// ------------------------------------------------------------------ adapters
//
// One interface, so the workloads are written once: insert (try_emplace semantics -- it must NOT
// overwrite), bump (++m[k], so it inserts a miss and returns the new value), count, erase, sum
// (iterate every value), size. Every adapter is checked against the group index operation for
// operation by `check` before any timing means anything; that is what caught Verstable's _insert
// being insert_or_assign.

template <typename Map>
struct stl_like {
    // Whether reserve() below really reserves. A map that cannot be told its size in advance would
    // otherwise measure its own growth in a workload whose whole point is that growth is out of it.
    static constexpr bool reserves = true;

    Map m{};
    void reserve(std::size_t n) {
        m.reserve(n);
    }
    template <typename K>
    void insert(K const& k, std::size_t v) {
        m.try_emplace(k, v);
    }
    // ++m[k] would do, but the 64 byte value is a struct that converts to and from size_t rather
    // than one with an operator++ of its own.
    template <typename K>
    auto bump(K const& k) -> std::size_t {
        auto& v = m[k];
        auto const next = static_cast<std::size_t>(v) + 1;
        v = next;
        return next;
    }
    template <typename K>
    auto count(K const& k) const -> std::size_t {
        return static_cast<std::size_t>(m.count(k));
    }
    template <typename K>
    void erase(K const& k) {
        m.erase(k);
    }
    auto sum() const -> std::size_t {
        auto s = std::size_t{0};
        for (auto const& kv : m) {
            s += static_cast<std::size_t>(kv.second);
        }
        return s;
    }
    auto size() const -> std::size_t {
        return static_cast<std::size_t>(m.size());
    }
};

#define UDM_MAP(id, label, ...)                    \
    template <typename Key, typename Val>          \
    struct id : stl_like<__VA_ARGS__> {            \
        static constexpr char const* name = label; \
    }

UDM_MAP(a_udm, "udm", ankerl::unordered_dense::map<Key, Val UDM_HASH(Key)>);
UDM_MAP(a_udm411, "udm-4.11", udmbase::unordered_dense::map<Key, Val UDM_HASH(Key)>);
// The two shapes of this map a caller can opt into: stable references through a segmented value
// container, and every block of 2 MB and up on a huge page. Both are the shipped defaults of their
// aliases, except the segment size, which is the one doc/usage.md recommends for the page.
UDM_MAP(a_udm_seg, "udm-segmented", ankerl::unordered_dense::segmented_map<Key, Val UDM_HASH(Key)>);
#ifdef UDM_HAVE_HUGE
UDM_MAP(a_udm_huge, "udm-huge", ankerl::unordered_dense::huge_page::map<Key, Val UDM_HASH(Key)>);
UDM_MAP(a_udm_seghuge,
        "udm-seg-huge",
        ankerl::unordered_dense::huge_page::segmented_map<Key,
                                                          Val,
                                                          UDM_HASH_HERE(Key),
                                                          std::equal_to<Key>,
                                                          ankerl::unordered_dense::bucket_type::group,
                                                          (std::size_t{16} << 20U)>);
#endif
UDM_MAP(a_boost, "boost", boost::unordered_flat_map<Key, Val UDM_HASH(Key)>);
UDM_MAP(a_boost_own, "boost-own", boost::unordered_flat_map<Key, Val>);
UDM_MAP(a_boost_node, "boost-node", boost::unordered_node_map<Key, Val UDM_HASH(Key)>);
UDM_MAP(a_std, "std", std::unordered_map<Key, Val UDM_HASH(Key)>);
#ifdef UDM_HAVE_ABSL
UDM_MAP(a_absl, "absl", absl::flat_hash_map<Key, Val UDM_HASH(Key)>);
UDM_MAP(a_absl_own, "absl-own", absl::flat_hash_map<Key, Val>);
UDM_MAP(a_absl_node, "absl-node", absl::node_hash_map<Key, Val UDM_HASH(Key)>);
#endif
#ifdef UDM_HAVE_F14
UDM_MAP(a_f14value, "f14-value", folly::F14ValueMap<Key, Val UDM_HASH(Key)>);
UDM_MAP(a_f14vector, "f14-vector", folly::F14VectorMap<Key, Val UDM_HASH(Key)>);
UDM_MAP(a_f14node, "f14-node", folly::F14NodeMap<Key, Val UDM_HASH(Key)>);
#endif
#ifdef UDM_HAVE_EMHASH
UDM_MAP(a_emhash8, "emhash8", emhash8::HashMap<Key, Val UDM_HASH(Key)>);
UDM_MAP(a_emilib, "emilib", emilib::HashMap<Key, Val UDM_HASH(Key)>);
#endif
#ifdef UDM_HAVE_INDIVI
UDM_MAP(a_indivi_u, "indivi-u", indivi::flat_umap<Key, Val UDM_HASH(Key)>);
UDM_MAP(a_indivi_w, "indivi-w", indivi::flat_wmap<Key, Val UDM_HASH(Key)>);
#endif
#undef UDM_MAP

#ifdef UDM_HAVE_VERSTABLE
// Verstable's generated names are macro-pasted, so a template cannot name them and the wrapper is
// written out. Its buckets are raw malloc memory that is never constructed, which is why the key
// has to be trivially copyable and there is no string row for it.
template <typename Key, typename Val>
struct a_verstable {
    static_assert(std::is_same_v<Key, std::uint64_t> && std::is_same_v<Val, std::size_t>,
                  "fixed by the macro template it wraps: only the uint64_t key and the 8 byte value");
    static constexpr char const* name = "verstable";
    vtab t{};
    a_verstable() {
        vtab_init(&t);
    }
    ~a_verstable() {
        vtab_cleanup(&t);
    }
    a_verstable(a_verstable const&) = delete;
    auto operator=(a_verstable const&) -> a_verstable& = delete;

    static constexpr bool reserves = true;
    void reserve(std::size_t n) {
        vtab_reserve(&t, n);
    }

    // _insert replaces an existing value, like insert_or_assign; the honest counterpart of
    // try_emplace is _get_or_insert.
    void insert(std::uint64_t k, std::size_t v) {
        vtab_get_or_insert(&t, k, v);
    }
    auto bump(std::uint64_t k) -> std::size_t {
        return ++vtab_get_or_insert(&t, k, std::size_t{0}).data->val;
    }
    auto count(std::uint64_t k) -> std::size_t {
        return vtab_is_end(vtab_get(&t, k)) ? 0U : 1U;
    }
    void erase(std::uint64_t k) {
        vtab_erase(&t, k);
    }
    auto sum() -> std::size_t {
        auto s = std::size_t{0};
        for (auto i = vtab_first(&t); !vtab_is_end(i); i = vtab_next(i)) {
            s += i.data->val;
        }
        return s;
    }
    auto size() const -> std::size_t {
        return vtab_size(&t);
    }
};
#endif

#ifdef UDM_HAVE_IHTAB
// ihtab stores whole elements and is told how to hash and compare them, so the "map" is a pair
// struct whose hash and equality look only at the key.
struct iht_entry {
    std::uint64_t key;
    std::size_t value;
};
struct iht_hash {
    auto operator()(iht_entry const& e) const -> std::size_t {
        return same_hash<std::uint64_t>{}(e.key);
    }
};
struct iht_eq {
    auto operator()(iht_entry const& a, iht_entry const& b) const -> bool {
        return a.key == b.key;
    }
};

template <typename Key, typename Val>
struct a_ihtab {
    static_assert(std::is_same_v<Key, std::uint64_t> && std::is_same_v<Val, std::size_t>,
                  "fixed by the macro template it wraps: only the uint64_t key and the 8 byte value");
    static constexpr char const* name = "ihtab";
    iht::ihtab<iht_entry, iht_hash, iht_eq> t{8};

    // Sized at construction and never afterwards, so this cannot honour the request. Said out loud
    // rather than silently ignored: `reserves` is what makes the harness print the difference.
    static constexpr bool reserves = false;
    void reserve(std::size_t /*n*/) {}

    void insert(std::uint64_t k, std::size_t v) {
        auto e = iht_entry{k, v};
        iht_entry* r = nullptr;
        if (!t.perform(e, iht::INSERT, &r)) {
            *r = e;
        }
    }
    auto bump(std::uint64_t k) -> std::size_t {
        auto e = iht_entry{k, 0};
        iht_entry* r = nullptr;
        if (!t.perform(e, iht::INSERT, &r)) {
            *r = e;
        }
        return ++r->value;
    }
    auto count(std::uint64_t k) -> std::size_t {
        auto e = iht_entry{k, 0};
        iht_entry* r = nullptr;
        return t.perform(e, iht::FIND, &r) ? 1U : 0U;
    }
    void erase(std::uint64_t k) {
        auto e = iht_entry{k, 0};
        iht_entry* r = nullptr;
        t.perform(e, iht::DELETE, &r);
    }
    auto sum() -> std::size_t {
        auto s = std::size_t{0};
        for (auto& e : t) {
            s += e.value;
        }
        return s;
    }
    auto size() const -> std::size_t {
        return t.els_count();
    }
};
#endif

// ------------------------------------------------------------------ the lists
// The group index is index 0 everywhere: it is the reference every ratio is taken against, and the
// map `check` compares the others to.

template <typename T>
struct pop_front;
template <typename First, typename... Rest>
struct pop_front<std::tuple<First, Rest...>> {
    using type = std::tuple<Rest...>;
};
template <typename T>
using pop_front_t = typename pop_front<T>::type;

template <typename Key, typename Val = std::size_t>
struct maps_for;

// Leading commas, because every entry after the first four is conditional on a checkout being
// there and a trailing comma before `>` is not a list.
#ifdef UDM_HAVE_ABSL
#    define UDM_LIST_ABSL(K, V) , a_absl<K, V> UDM_LIST_ABSL_OWN(K, V)
#    define UDM_LIST_ABSL_NODE(K, V) , a_absl_node<K, V>
#else
#    define UDM_LIST_ABSL(K, V)
#    define UDM_LIST_ABSL_NODE(K, V)
#endif
#ifdef UDM_HAVE_F14
#    define UDM_LIST_F14(K, V) , a_f14value<K, V>, a_f14vector<K, V>
#    define UDM_LIST_F14_NODE(K, V) , a_f14node<K, V>
#else
#    define UDM_LIST_F14(K, V)
#    define UDM_LIST_F14_NODE(K, V)
#endif
#ifdef UDM_HAVE_EMHASH
#    define UDM_LIST_EMHASH(K, V) , a_emhash8<K, V>, a_emilib<K, V>
#else
#    define UDM_LIST_EMHASH(K, V)
#endif
#ifdef UDM_HAVE_INDIVI
#    define UDM_LIST_INDIVI(K, V) , a_indivi_u<K, V>, a_indivi_w<K, V>
#else
#    define UDM_LIST_INDIVI(K, V)
#endif
#ifdef UDM_HAVE_VERSTABLE
#    define UDM_LIST_VERSTABLE(K, V) , a_verstable<K, V>
#else
#    define UDM_LIST_VERSTABLE(K, V)
#endif
#ifdef UDM_HAVE_IHTAB
#    define UDM_LIST_IHTAB(K, V) , a_ihtab<K, V>
#else
#    define UDM_LIST_IHTAB(K, V)
#endif

// The three shapes of this map a caller opts into, rather than three more indexes to compare. They
// are off unless a harness asks for them, because maps.cpp's `memory` mode counts with mallinfo2(),
// which cannot see the huge page allocator's raw mmap and would report those two rows at near zero
// -- and because maps.sh's own numbers are a fixed list of maps that should not move under it.
#if defined(UDM_VARIANTS)
#    ifdef UDM_HAVE_HUGE
#        define UDM_LIST_HUGE(K, V) , a_udm_huge<K, V>, a_udm_seghuge<K, V>
#    else
#        define UDM_LIST_HUGE(K, V)
#    endif
#    define UDM_LIST_VARIANTS(K, V) , a_udm_seg<K, V> UDM_LIST_HUGE(K, V)
#else
#    define UDM_LIST_VARIANTS(K, V)
#endif
// `boost-own` and `absl-own` are the same-hash run's controls -- the two maps whose own hash a
// caller actually gets. With UDM_DEFAULT_HASH every row is already that, so they would be duplicates.
#if defined(UDM_DEFAULT_HASH)
#    define UDM_LIST_OWN(K, V)
#    define UDM_LIST_ABSL_OWN(K, V)
#else
#    define UDM_LIST_OWN(K, V) , a_boost_own<K, V>
#    define UDM_LIST_ABSL_OWN(K, V) , a_absl_own<K, V>
#endif

#define UDM_LIST_FLAT(K, V)                                                              \
    a_udm<K, V> UDM_LIST_VARIANTS(K, V), a_udm411<K, V>, \
        a_boost<K, V> UDM_LIST_OWN(K, V) UDM_LIST_ABSL(K, V) UDM_LIST_F14(K, V) UDM_LIST_EMHASH(K, V) UDM_LIST_INDIVI(K, V)
#define UDM_LIST_NODE(K, V) a_std<K, V>, a_boost_node<K, V> UDM_LIST_ABSL_NODE(K, V) UDM_LIST_F14_NODE(K, V)

template <typename Val>
struct maps_for<std::uint64_t, Val> {
    using key = std::uint64_t;
    // Verstable's value type is fixed by the macro template it is generated from, and ihtab's by
    // the element struct it is handed, so those two are in the list only for the eight byte value.
    using fixed_value_maps = std::conditional_t<std::is_same_v<Val, std::size_t>,
                                                std::tuple<int UDM_LIST_VERSTABLE(key, Val) UDM_LIST_IHTAB(key, Val)>,
                                                std::tuple<int>>;
    using type = decltype(std::tuple_cat(std::declval<std::tuple<UDM_LIST_FLAT(key, Val)>>(),
                                         // drop the `int` placeholder that made the leading commas legal
                                         std::declval<pop_front_t<fixed_value_maps>>(),
                                         std::declval<std::tuple<UDM_LIST_NODE(key, Val)>>()));
};

// Verstable and ihtab want a trivially copyable key, so neither has a string row.
template <typename Val>
struct maps_for<std::string, Val> {
    using key = std::string;
    using type = std::tuple<UDM_LIST_FLAT(key, Val), UDM_LIST_NODE(key, Val)>;
};

// ------------------------------------------------------------------ keys
//
// Three disjoint pools, so a miss is absent by construction rather than by luck and is not the
// neighbour of a key that is present, and so churn never builds a key inside the timed region.
constexpr auto present_range = std::uint64_t{0};
constexpr auto absent_range = std::uint64_t{1} << 63U;
constexpr auto spare_range = std::uint64_t{1} << 62U;

template <typename Key>
auto make_key(std::uint64_t v) -> Key {
    return workloads::key_source<Key>::get(v);
}
template <>
auto make_key<std::uint64_t>(std::uint64_t v) -> std::uint64_t {
    return v; // already the output of an rng; the scrambler exists for sequential values
}

template <typename Key>
auto make_keys(std::uint64_t range, std::size_t n, std::uint64_t seed) -> std::vector<Key> {
    auto out = std::vector<Key>();
    out.reserve(n);
    auto r = ankerl::nanobench::Rng(seed);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(make_key<Key>((r() >> 2U) | range));
    }
    return out;
}

template <typename Key>
struct pools {
    std::vector<Key> present, absent, spare;
    explicit pools(std::size_t n)
        : present(make_keys<Key>(present_range, n, 1))
        , absent(make_keys<Key>(absent_range, n, 2))
        , spare(make_keys<Key>(spare_range, n, 3)) {}
};

// ------------------------------------------------------------------ workloads

template <typename Map, typename Key>
void fill(Map& m, pools<Key> const& p) {
    for (auto const& k : p.present) {
        m.insert(k, 1);
    }
}

template <typename Map, typename Key>
auto build(pools<Key> const& p) -> std::size_t {
    auto m = Map();
    fill(m, p);
    return m.size();
}

// The same build with the size known in advance, which takes growth and the rehash out of it and
// leaves the pure insert path: a probe that misses, a value appended, a slot pointed at it. That is
// the thing this map is slowest at relative to a flat one -- 97.8 instructions under clang against
// boost's 64 on 2026-09-08 -- and there was no tool in the repository that measured it on its own.
//
// A map that cannot reserve (Map::reserves is false) measures its own growth here instead, which is
// a different quantity; maps_one.cpp prints which it was.
template <typename Map, typename Key>
auto build_reserved(pools<Key> const& p) -> std::size_t {
    auto m = Map();
    m.reserve(p.present.size());
    fill(m, p);
    return m.size();
}

// The rngs outlive the epochs on purpose. A benchmark whose per-epoch batch is small enough to
// memorise must advance its own randomness, or a TAGE-style predictor learns the sequence of hits
// and misses and flatters whichever probe has the most branches in it -- measured at 2.7x once.
struct lookup_state {
    ankerl::nanobench::Rng rng{5};
    ankerl::nanobench::Rng coin{99};
};

enum class asking { hits, half, misses };

template <typename Map, typename Key>
auto lookups(Map& m, pools<Key> const& p, lookup_state& st, asking what, std::size_t n) -> std::size_t {
    auto acc = std::size_t{0};
    for (std::size_t i = 0; i < n; ++i) {
        auto const at = static_cast<std::size_t>(((st.rng() >> 32U) * p.present.size()) >> 32U);
        auto const hit = what == asking::hits || (what == asking::half && (st.coin() & 1U) != 0);
        acc += m.count(hit ? p.present[at] : p.absent[at]);
    }
    return acc;
}

// ++m[k] on a key that is already there: the writing lookup, which finds the key, does not place
// anything, and on this map also gets move_home(). It is the commonest map operation there is and
// it is where an always_inline placement path is paid for without being used (clang 73.2
// instructions against 48.4 without it), so it is measured on its own rather than inside churn.
//
// Draws its keys the way lookups() does, from a state that outlives the epoch: a batch small enough
// to memorise, replayed, is learned by the branch predictor.
template <typename Map, typename Key>
auto bump_present(Map& m, pools<Key> const& p, lookup_state& st, std::size_t n) -> std::size_t {
    auto acc = std::size_t{0};
    for (std::size_t i = 0; i < n; ++i) {
        auto const at = static_cast<std::size_t>(((st.rng() >> 32U) * p.present.size()) >> 32U);
        acc += m.bump(p.present[at]);
    }
    return acc;
}

// Erase one, insert one, holding the size exactly where it is. The inserted key is one the map has
// never held: recycling keys out of a small spare pool halves the measured drift, because a key
// that comes back soon tends to land in the home it just left.
template <typename Map, typename Key>
void churn(Map& m,
           std::vector<Key>& present,
           std::vector<Key>& spare,
           ankerl::nanobench::Rng& rng,
           std::size_t ops,
           std::size_t& tick) {
    for (std::size_t i = 0; i < ops; ++i) {
        auto const slot = static_cast<std::size_t>(((rng() >> 32U) * present.size()) >> 32U);
        auto const j = tick++ % spare.size();
        m.erase(present[slot]);
        m.insert(spare[j], 1);
        std::swap(present[slot], spare[j]);
    }
}

template <typename Map, typename Key>
auto insert_erase(Map& m, pools<Key>& p, ankerl::nanobench::Rng& rng, std::size_t ops, std::size_t& tick) -> std::size_t {
    auto acc = std::size_t{0};
    for (std::size_t i = 0; i < ops; ++i) {
        auto const a = static_cast<std::size_t>(((rng() >> 32U) * p.present.size()) >> 32U);
        acc += m.bump(p.present[a]);
        m.erase(p.absent[a]);
        auto const b = static_cast<std::size_t>(((rng() >> 32U) * p.present.size()) >> 32U);
        auto const j = tick++ % p.spare.size();
        m.erase(p.present[b]);
        acc += m.bump(p.spare[j]);
        std::swap(p.present[b], p.spare[j]);
    }
    return acc;
}

} // namespace udm_maps
