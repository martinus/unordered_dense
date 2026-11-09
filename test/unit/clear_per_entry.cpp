#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/hashers.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// clear() on a table holding fewer than one entry per 16 groups takes each entry out of the index
// the way an erase does, instead of resetting every group (#381). The two must leave the same index:
// every fingerprint and every counter zero, the bucket count unchanged, and a table that then
// behaves, insert for insert, like one that was never filled. Nothing outside the map can tell the
// two paths apart except the bytes of index() and how often the hash is called, so that is what
// these read.

namespace {

// table::clear_per_entry_groups
constexpr std::size_t clear_k = 16;

std::size_t g_clear_hashes = 0;

struct clear_counting_hash {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(std::uint64_t key) const noexcept -> std::uint64_t {
        ++g_clear_hashes;
        return key;
    }
};

// Throws on the call that takes this to zero; negative never throws.
int g_clear_hash_throw_in = -1;

// Not noexcept, so clear() has to survive it.
struct clear_throwing_hash {
    using is_avalanching = void;

    [[nodiscard]] auto operator()(std::uint64_t key) const -> std::uint64_t {
        if (g_clear_hash_throw_in >= 0 && g_clear_hash_throw_in-- == 0) {
            throw std::runtime_error("hash");
        }
        return key;
    }
};

template <typename Map>
constexpr std::size_t clear_slots = std::tuple_size_v<decltype(Map::bucket_type::m_fingerprints)>;

template <typename Map>
auto clear_groups(Map const& map) -> std::size_t {
    return map.bucket_count() / clear_slots<Map>;
}

// The hash returns the key, so a key spells out where it goes: `home` is the group, the low byte the
// fingerprint, `id` only makes keys distinct.
struct clear_key_maker {
    unsigned shift;

    [[nodiscard]] auto operator()(std::uint64_t home, std::uint64_t id, std::uint64_t fingerprint) const -> std::uint64_t {
        return (home << shift) | (id << 8U) | fingerprint;
    }
};

template <typename Map>
auto clear_keys(Map const& map) -> clear_key_maker {
    auto shift = 64U;
    for (auto groups = clear_groups(map); groups > 1; groups /= 2) {
        --shift;
    }
    return clear_key_maker{shift};
}

// No fingerprint and no counter set anywhere: what a fresh array and a full reset both are.
template <typename Map>
auto index_is_empty(Map const& map) -> bool {
    auto const idx = map.index();
    for (std::size_t i = 0; i < idx.size(); ++i) {
        auto const& block = idx.data()[i];
        for (auto fp : block.m_fingerprints) {
            if (fp != 0) {
                return false;
            }
        }
        for (auto c : block.m_overflows) {
            if (c != 0) {
                return false;
            }
        }
    }
    return true;
}

// Same fingerprints and counters in every group, and every full slot naming the same key. The value
// index of an empty slot is never read, so it is not compared.
template <typename Map>
auto same_index(Map const& a, Map const& b) -> bool {
    auto const ia = a.index();
    auto const ib = b.index();
    if (ia.size() != ib.size()) {
        return false;
    }
    for (std::size_t i = 0; i < ia.size(); ++i) {
        auto const& ba = ia.data()[i];
        auto const& bb = ib.data()[i];
        if (ba.m_fingerprints != bb.m_fingerprints || ba.m_overflows != bb.m_overflows) {
            return false;
        }
        for (std::size_t lane = 0; lane < ba.m_fingerprints.size(); ++lane) {
            if (ba.m_fingerprints[lane] != 0 && a.values()[static_cast<std::size_t>(ba.m_index[lane])].first !=
                                                    b.values()[static_cast<std::size_t>(bb.m_index[lane])].first) {
                return false;
            }
        }
    }
    return true;
}

constexpr std::uint64_t clear_fp = 0x11;
constexpr std::uint64_t clear_home = 3;
constexpr std::uint64_t clear_overflowers = 3; // homed in a full group, so placed in the next one

// The keys the table holds when it is cleared, and fills with again afterwards: a full home group,
// three keys of the same class that overflowed past it, and a few ordinary ones.
auto clear_survivors(clear_key_maker const& key) -> std::vector<std::uint64_t> {
    auto keys = std::vector<std::uint64_t>();
    for (std::uint64_t i = 0; i < 16 + clear_overflowers; ++i) {
        keys.push_back(key(clear_home, i, clear_fp));
    }
    for (std::uint64_t i = 0; i < 5; ++i) {
        keys.push_back(key(100 + (i * 7), 50 + i, 0x20 + i));
    }
    return keys;
}

// Fills the table with `bulk` keys spread over every group but the survivors' first ten, then the
// survivors, then erases the bulk keys again unless told to keep them, and clears.
template <typename Map>
void grow_shrink_clear_refill(std::size_t bulk, bool keep_bulk) {
    auto map = Map();
    map.reserve(100000);
    auto const buckets = map.bucket_count();
    auto const groups = clear_groups(map);
    auto const key = clear_keys(map);
    auto const bulk_key = [&](std::uint64_t i) {
        return key(10 + (i % (groups - 10)), 1000 + i, 0x40 + (i % 64));
    };
    for (std::uint64_t i = 0; i < bulk; ++i) {
        REQUIRE(map.try_emplace(bulk_key(i), i).second);
    }
    auto const survivors = clear_survivors(key);
    for (std::size_t i = 0; i < survivors.size(); ++i) {
        REQUIRE(map.try_emplace(survivors[i], i).second);
    }
    if (!keep_bulk) {
        for (std::uint64_t i = 0; i < bulk; ++i) {
            REQUIRE(map.erase(bulk_key(i)) == 1U);
        }
    }
    // the home group is full and its counter counts the overflowers
    REQUIRE(map.index().data()[clear_home].m_overflows[clear_fp & 7U] == clear_overflowers);
    REQUIRE((map.size() < groups / clear_k) == !keep_bulk);

    // one hash per entry, displaced ones included, when emptied entry by entry; none for a reset
    auto const held = map.size();
    g_clear_hashes = 0;
    map.clear();
    REQUIRE(g_clear_hashes == (keep_bulk ? 0U : held));
    REQUIRE(map.empty());
    REQUIRE(map.size() == 0U);
    REQUIRE(map.begin() == map.end());
    REQUIRE(map.bucket_count() == buckets);
    REQUIRE(index_is_empty(map));
    REQUIRE(!map.contains(survivors.front()));

    auto fresh = Map();
    fresh.reserve(100000);
    REQUIRE(fresh.bucket_count() == buckets);
    for (std::size_t i = 0; i < survivors.size(); ++i) {
        REQUIRE(map.try_emplace(survivors[i], i).second);
        REQUIRE(fresh.try_emplace(survivors[i], i).second);
    }
    REQUIRE(map.size() == survivors.size());
    REQUIRE(same_index(map, fresh));
    for (std::size_t i = 0; i < survivors.size(); ++i) {
        auto it = map.find(survivors[i]);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i);
        REQUIRE(!map.try_emplace(survivors[i], 0).second);
    }
    // misses: one of the crowded class at the crowded home, one elsewhere, one of a bulk key
    REQUIRE(!map.contains(key(clear_home, 999, clear_fp)));
    REQUIRE(!map.contains(key(clear_home + 1, 999, clear_fp)));
    REQUIRE(!map.contains(bulk_key(0)));
    auto n = std::size_t{0};
    for (auto const& kv : map) {
        REQUIRE(kv.second == n);
        ++n;
    }
    REQUIRE(n == survivors.size());
}

} // namespace

TEST_CASE_MAP("clear_per_entry_leaves_the_index_as_fresh", std::uint64_t, std::uint64_t, clear_counting_hash) {
    SUBCASE("few entries: each slot emptied") {
        grow_shrink_clear_refill<map_t>(20000, false);
    }
    SUBCASE("many entries: every group reset") {
        grow_shrink_clear_refill<map_t>(20000, true);
    }
}

// Which path ran is visible as hash calls: one per entry for the per-entry clear, none for the reset.
// The boundary is one entry fewer than groups / 16.
TEST_CASE_MAP("clear_per_entry_boundary", std::uint64_t, std::uint64_t, clear_counting_hash) {
    auto map = map_t();
    map.reserve(100000);
    auto const groups = clear_groups(map);
    REQUIRE(groups >= clear_k * 4);
    auto const limit = groups / clear_k;
    for (auto const n : {limit - 1, limit, std::size_t{1}, limit + 1}) {
        for (std::uint64_t i = 0; i < n; ++i) {
            map.try_emplace(i * UINT64_C(0x9E3779B97F4A7C15), i);
        }
        REQUIRE(map.size() == n);
        g_clear_hashes = 0;
        map.clear();
        REQUIRE(g_clear_hashes == (n < limit ? n : 0U));
        REQUIRE(map.empty());
        REQUIRE(index_is_empty(map));
        REQUIRE(clear_groups(map) == groups);
    }

    // a table of fewer than 16 groups always resets
    auto small = map_t();
    small.try_emplace(1, 1);
    REQUIRE(clear_groups(small) < clear_k);
    g_clear_hashes = 0;
    small.clear();
    REQUIRE(g_clear_hashes == 0U);
    REQUIRE(index_is_empty(small));
}

// A counter that reached 255 is never decremented again, so emptying the entries one by one cannot
// bring it back to zero; meeting one makes clear() reset everything instead.
TEST_CASE_MAP("clear_per_entry_resets_a_saturated_counter", std::uint64_t, std::uint64_t, test::identity_hash) {
    auto map = map_t();
    map.reserve(150000);
    auto const key = clear_keys(map);
    for (std::uint64_t i = 0; i < 300; ++i) {
        REQUIRE(map.try_emplace(key(0, i, clear_fp), i).second);
    }
    REQUIRE(map.index().data()[0].m_overflows[clear_fp & 7U] == 255);
    REQUIRE(map.size() < clear_groups(map) / clear_k);
    map.clear();
    REQUIRE(index_is_empty(map));
    REQUIRE(map.try_emplace(key(0, 1, clear_fp), 1).second);
    REQUIRE(map.index().data()[0].m_overflows[clear_fp & 7U] == 0);
}

// The saturation test, read through the hash count: 254 entries past the home group is an exact
// counter and is walked to the end; 255 is saturated, and the walk gives up at the first entry that
// passes it. And a counter left at 255 by entries erased since is still reset, as long as one of
// the entries held passes it.
TEST_CASE_MAP("clear_per_entry_at_254_and_255", std::uint64_t, std::uint64_t, clear_counting_hash) {
    for (std::uint64_t const overflowers : {254U, 255U}) {
        auto map = map_t();
        map.reserve(150000);
        auto const key = clear_keys(map);
        for (std::uint64_t i = 0; i < 16 + overflowers; ++i) {
            REQUIRE(map.try_emplace(key(0, i, clear_fp), i).second);
        }
        REQUIRE(map.index().data()[0].m_overflows[clear_fp & 7U] == overflowers);
        REQUIRE(map.size() < clear_groups(map) / clear_k);
        g_clear_hashes = 0;
        map.clear();
        // the 16 at home pass nothing; with 255 the next one meets the saturated counter
        REQUIRE(g_clear_hashes == (overflowers == 254 ? std::size_t{270} : std::size_t{17}));
        REQUIRE(index_is_empty(map));
    }

    auto map = map_t();
    map.reserve(150000);
    auto const key = clear_keys(map);
    for (std::uint64_t i = 0; i < 300; ++i) {
        REQUIRE(map.try_emplace(key(0, i, clear_fp), i).second);
    }
    for (std::uint64_t i = 36; i < 300; ++i) {
        REQUIRE(map.erase(key(0, i, clear_fp)) == 1U);
    }
    REQUIRE(map.index().data()[0].m_overflows[clear_fp & 7U] == 255);
    map.clear();
    REQUIRE(index_is_empty(map));
}

// A key changed in place sends the walk down a sequence its insert never counted in. The group it
// starts in is one away from the real one and shares the fingerprint, so the walk would find the
// value there -- after taking a counter of zero down to 255. It resets instead.
TEST_CASE_MAP("clear_per_entry_with_a_mutated_key", std::uint64_t, std::uint64_t, test::identity_hash) {
    auto map = map_t();
    map.reserve(10000);
    auto const key = clear_keys(map);
    map.try_emplace(key(5, 1, clear_fp), 1);
    map.begin()->first = key(4, 1, clear_fp); // home 4, whose next group is 5
    REQUIRE(map.size() < clear_groups(map) / clear_k);
    map.clear();
    REQUIRE(map.empty());
    REQUIRE(index_is_empty(map));
}

TEST_CASE_MAP("clear_per_entry_survives_a_throwing_hash", std::uint64_t, std::uint64_t, clear_throwing_hash) {
    // The throw comes after the full home group and two of the overflowers are already out of the
    // index, so the reset has to start from a half emptied one.
    auto map = map_t();
    map.reserve(10000);
    auto const survivors = clear_survivors(clear_keys(map));
    for (std::size_t i = 0; i < survivors.size(); ++i) {
        map.try_emplace(survivors[i], i);
    }
    REQUIRE(map.size() < clear_groups(map) / clear_k);
    g_clear_hash_throw_in = 18;
    REQUIRE_NOTHROW(map.clear());
    REQUIRE(g_clear_hash_throw_in == -1);
    g_clear_hash_throw_in = -1;
    REQUIRE(map.empty());
    REQUIRE(index_is_empty(map));
    REQUIRE(map.try_emplace(7, 7).second);
    REQUIRE(map.contains(7));
}

// With the library's own string hash, through a set.
TEST_CASE_SET("clear_per_entry_strings", std::string) {
    auto set = set_t();
    for (std::size_t i = 0; i < 100000; ++i) {
        set.insert("key " + std::to_string(i));
    }
    for (std::size_t i = 10; i < 100000; ++i) {
        REQUIRE(set.erase("key " + std::to_string(i)) == 1U);
    }
    auto const buckets = set.bucket_count();
    set.clear();
    REQUIRE(set.empty());
    REQUIRE(set.bucket_count() == buckets);
    auto const idx = set.index();
    for (std::size_t i = 0; i < idx.size(); ++i) {
        for (auto fp : idx.data()[i].m_fingerprints) {
            REQUIRE(fp == 0);
        }
        for (auto c : idx.data()[i].m_overflows) {
            REQUIRE(c == 0);
        }
    }
    for (std::size_t i = 0; i < 20; ++i) {
        REQUIRE(set.insert("key " + std::to_string(i)).second);
    }
    for (std::size_t i = 0; i < 20; ++i) {
        REQUIRE(set.contains("key " + std::to_string(i)));
    }
    REQUIRE(!set.contains("key 20"));
    REQUIRE(set.size() == 20U);
}
