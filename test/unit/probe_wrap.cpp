#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstdint>
#include <set>

// The vector probe reads four buckets from any bucket index, so the array carries three sentinel
// buckets past its end. A window that starts in the last three buckets and decides nothing has to
// move on to bucket zero by however many *real* buckets it saw, not by four -- in the probe, where
// the key is then found or placed on the far side of the wrap, and in the erase fixup, which looks
// for the bucket that points at the moved last value the same way. None of that happens unless a
// long robin hood chain runs across the end of the array, so this builds one on purpose: a hash
// that returns the key itself, and keys that spell out the bucket and fingerprint they want.

namespace {

struct identity_hash {
    using is_avalanching = void;
    auto operator()(uint64_t key) const noexcept -> uint64_t {
        return key;
    }
};

using wrap_map_t = ankerl::unordered_dense::map<uint64_t, uint64_t, identity_hash>;

struct key_maker {
    unsigned shift;

    // home is the bucket the key lands in, fingerprint its low byte; id makes keys distinct
    auto operator()(uint64_t home, uint64_t id, uint64_t fingerprint) const -> uint64_t {
        return (home << shift) | (id << 8U) | fingerprint;
    }
};

auto keys_of(wrap_map_t const& map) -> std::set<uint64_t> {
    auto keys = std::set<uint64_t>();
    for (auto const& kv : map) {
        keys.insert(kv.first);
    }
    return keys;
}

} // namespace

TEST_CASE("probe_wrap") {
    auto map = wrap_map_t();
    map.reserve(100);
    auto const n = map.bucket_count();
    REQUIRE(n >= 8);
    REQUIRE((n & (n - 1)) == 0);
    auto shift = unsigned{64};
    for (auto b = n; b > 1; b /= 2) {
        --shift;
    }
    auto const key = key_maker{shift};
    auto expected = std::set<uint64_t>();
    auto insert = [&](uint64_t k) {
        REQUIRE(map.try_emplace(k, k).second);
        expected.insert(k);
    };

    // a chain of 30 keys whose home is the fourth-to-last bucket: they occupy the last four
    // buckets and then, wrapped, buckets 0 to 25, with distances 1 to 30
    for (uint64_t id = 0; id < 30; ++id) {
        insert(key(n - 4, id, 0x11));
    }
    REQUIRE(map.bucket_count() == n);
    REQUIRE(keys_of(map) == expected);

    // Absent keys whose home is in the last three buckets. Every real bucket of their first window
    // holds a chain element that is further from home than the key would be, the rest of the window
    // is sentinel, so nothing is decided and the probe has to wrap.
    for (uint64_t home = n - 3; home < n; ++home) {
        for (uint64_t fp = 0; fp < 256; fp += 51) {
            REQUIRE(map.find(key(home, 999, fp)) == map.end());
            REQUIRE(!map.contains(key(home, 999, fp)));
            REQUIRE(map.count(key(home, 999, fp)) == 0);
        }
    }

    // Placing such a key walks the same way and puts it behind the chain, on the far side.
    auto const b = key(n - 1, 7, 0x33);
    insert(b);
    REQUIRE(map.find(b) != map.end());
    REQUIRE(map.find(b)->second == b);
    REQUIRE(map.try_emplace(b, 0).second == false);
    REQUIRE(keys_of(map) == expected);

    // Erasing it finds it the same way; afterwards it is gone and the chain is intact.
    REQUIRE(map.erase(b) == 1);
    expected.erase(b);
    REQUIRE(map.find(b) == map.end());
    REQUIRE(keys_of(map) == expected);

    // The erase fixup: the last value's home is in the last three buckets, its bucket is beyond the
    // wrap. Erasing something else moves it, and the scan for its bucket has to wrap to find it.
    auto const last = key(n - 2, 8, 0x44);
    insert(last);
    REQUIRE(map.erase(key(n - 4, 5, 0x11)) == 1);
    expected.erase(key(n - 4, 5, 0x11));
    REQUIRE(map.find(last) != map.end());
    REQUIRE(map.find(last)->second == last);
    REQUIRE(keys_of(map) == expected);
    REQUIRE(map.erase(last) == 1);
    expected.erase(last);
    REQUIRE(keys_of(map) == expected);

    // Take the chain down from the front, so that what is left keeps shifting across the wrap.
    for (uint64_t id = 0; id < 30; ++id) {
        auto k = key(n - 4, id, 0x11);
        if (expected.erase(k) != 0) {
            REQUIRE(map.erase(k) == 1);
        }
        REQUIRE(keys_of(map) == expected);
        for (auto e : expected) {
            REQUIRE(map.find(e) != map.end());
        }
    }
    REQUIRE(map.empty());
    REQUIRE(map.bucket_count() == n);

    // and the sentinels were never taken for empty buckets: the array is as usable as before
    for (uint64_t id = 0; id < 30; ++id) {
        insert(key(n - 1, id, 0x55));
    }
    REQUIRE(keys_of(map) == expected);
    REQUIRE(map.bucket_count() == n);
}
