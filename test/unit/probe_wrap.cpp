#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/hashers.h>

#include <cstdint>
#include <set>

// A probe sequence that starts in the last group continues at the front of the array, and every
// walk along it -- the probe, the placement, the counters an erase takes down, and the scan for
// the slot pointing at a value -- has to agree on where it goes. That is one mask, so the way to
// see a mistake in it is a chain that actually crosses the end: keys that spell out the group and
// fingerprint they want, through a hash that returns the key, enough of them to overflow the last
// group twice over.

namespace {

using wrap_map_t = ankerl::unordered_dense::map<uint64_t, uint64_t, test::identity_hash>;

struct key_maker {
    unsigned shift;

    // home is the group the key lands in, fingerprint its low byte; id makes keys distinct
    auto operator()(uint64_t home, uint64_t id, uint64_t fingerprint) const -> uint64_t {
        return (home << shift) | (id << 8U) | fingerprint;
    }
};

void require_exactly(wrap_map_t const& map, std::set<uint64_t> const& expected) {
    REQUIRE(map.size() == expected.size());
    for (auto e : expected) {
        REQUIRE(map.find(e) != map.end());
    }
}

} // namespace

TEST_CASE("probe_wrap") {
    auto map = wrap_map_t();
    map.reserve(100);
    auto const groups = map.bucket_count() / 16;
    REQUIRE(groups >= 4);
    REQUIRE((groups & (groups - 1)) == 0);
    auto shift = unsigned{64};
    for (auto g = groups; g > 1; g /= 2) {
        --shift;
    }
    auto const key = key_maker{shift};
    auto const last = groups - 1;
    auto expected = std::set<uint64_t>();
    auto insert = [&](uint64_t k) {
        REQUIRE(map.try_emplace(k, k).second);
        expected.insert(k);
    };

    // Forty keys whose home is the last group, all of one fingerprint: sixteen fill it, the rest
    // overflow across the wrap into groups 0 and 2, counted in the counters they pass.
    for (uint64_t id = 0; id < 40; ++id) {
        insert(key(last, id, 0x11));
    }
    auto const buckets = map.bucket_count();
    require_exactly(map, expected);

    // Absent keys with the same home. Those sharing the chain's counter walk it to the end and
    // back round; the others stop in the home group. Neither may be found.
    for (uint64_t fp = 1; fp < 256; fp += 17) {
        auto const absent = key(last, 999, fp);
        REQUIRE(map.find(absent) == map.end());
        REQUIRE(!map.contains(absent));
        REQUIRE(map.count(absent) == 0);
    }

    // A key of another fingerprint class placed from the full home group lands past the wrap too,
    // counted in the last group and in group 0 on its way.
    auto const b = key(last, 7, 0x33);
    insert(b);
    REQUIRE(map.find(b)->second == b);
    REQUIRE(map.try_emplace(b, 0).second == false);
    require_exactly(map, expected);

    // Erasing it walks the same way, taking the counters back down; afterwards it is gone and the
    // chain is intact.
    REQUIRE(map.erase(b) == 1);
    expected.erase(b);
    REQUIRE(map.find(b) == map.end());
    require_exactly(map, expected);

    // Erase by iterator looks for the slot pointing at the value's index, from its home. The first
    // value has index 0 and sits in the last group; the last value moves into its place, and the
    // scan for that one's slot has to cross the wrap to find it.
    REQUIRE(map.begin()->first == key(last, 0, 0x11));
    map.erase(map.begin());
    expected.erase(key(last, 0, 0x11));
    require_exactly(map, expected);

    // Take the chain down from the front, so that what is left keeps being found across the wrap.
    for (uint64_t id = 0; id < 40; ++id) {
        auto k = key(last, id, 0x11);
        if (expected.erase(k) != 0) {
            REQUIRE(map.erase(k) == 1);
        }
        require_exactly(map, expected);
    }
    REQUIRE(map.empty());
    REQUIRE(map.bucket_count() == buckets);

    // and every counter came back down: the array is as usable as before
    for (uint64_t id = 0; id < 40; ++id) {
        insert(key(last, id, 0x55));
    }
    require_exactly(map, expected);
    REQUIRE(map.bucket_count() == buckets);
}
