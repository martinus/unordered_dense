#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <app/hashers.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>

// An erase takes its entry back out of every overflow counter its insert put it into, and until
// this test nothing checked that. Deleting the decrement outright leaves the whole suite green --
// measured, `scripts/mutate/mutate.py --replace` on that line reports SURVIVED against 771 cases.
//
// It survives because of the probe's termination bound. A counter that is too high never produces
// a wrong answer, only a longer walk: the probe carries on past the group it should have stopped
// at, finds nothing, and stops at the end of the array instead. That was a deliberate trade -- an
// unbounded probe was a hang, and a hostile hash could reach it -- and its price is exactly this,
// that a broken decrement is silent. A table that only ever churns would then get slower forever
// with every test still passing.
//
// What a stale counter does change is how far a *miss* walks, and that is observable from outside
// the map: count how often it asks KeyEqual. So this builds two tables that hold the same keys in
// the same slots, one of which reached that state by erasing a run of entries that had overflowed
// one group into the next, and requires the two to compare a miss equally often. That is the
// design's own claim -- an erase leaves the table as though the entry had never been inserted --
// in the one currency measurable without reaching inside the map.

namespace {

// Single threaded, so a plain counter. Reset before each measurement rather than at construction:
// building the table does comparisons of its own and they are not what is being counted.
size_t g_uncount_comparisons = 0;

struct uncount_counting_equal {
    [[nodiscard]] auto operator()(std::uint64_t a, std::uint64_t b) const noexcept -> bool {
        ++g_uncount_comparisons;
        return a == b;
    }
};

using uncount_map_t = ankerl::unordered_dense::map<std::uint64_t, std::uint64_t, test::identity_hash, uncount_counting_equal>;

constexpr size_t uncount_slots = std::tuple_size_v<decltype(uncount_map_t::bucket_type::m_fingerprints)>;

// The hash returns the key, so a key spells out where it goes: `home` picks the group, the low
// byte is the fingerprint, and `id` only makes keys distinct.
struct uncount_key_maker {
    unsigned shift;

    [[nodiscard]] auto operator()(std::uint64_t home, std::uint64_t id, std::uint64_t fingerprint) const -> std::uint64_t {
        return (home << shift) | (id << 8U) | fingerprint;
    }
};

auto uncount_keys(uncount_map_t const& map) -> uncount_key_maker {
    auto shift = 64U;
    for (auto groups = map.bucket_count() / uncount_slots; groups > 1; groups /= 2) {
        --shift;
    }
    return uncount_key_maker{shift};
}

auto comparisons_for(uncount_map_t const& map, std::uint64_t key) -> size_t {
    g_uncount_comparisons = 0;
    REQUIRE(map.count(key) == 0U);
    return g_uncount_comparisons;
}

// One fingerprint for every key, so that a probe which walks one group too far has something there
// to compare against -- a group whose fingerprints all differ would be walked for free and the
// stale counter would stay invisible.
constexpr std::uint64_t uncount_fp = 0x11;
constexpr std::uint64_t uncount_home = 3;
constexpr std::uint64_t uncount_next = 4; // where an entry homed in `uncount_home` overflows to
constexpr std::uint64_t uncount_overflowers = 3;

// What to do with the entries that overflow the home group: none at all, send them past and erase
// them again (so the survivors are what `none` builds directly), or send them past and keep them.
enum class overflow { none, erased, live };

auto build(overflow mode) -> uncount_map_t {
    auto map = uncount_map_t();
    map.reserve(1000);
    auto const key = uncount_keys(map);

    // fill the home group to the brim
    for (std::uint64_t i = 0; i < uncount_slots; ++i) {
        REQUIRE(map.try_emplace(key(uncount_home, i, uncount_fp), i).second);
    }
    // give the next group residents of its own, so a probe that wrongly carries on has work to do
    for (std::uint64_t i = 0; i < uncount_slots / 2; ++i) {
        REQUIRE(map.try_emplace(key(uncount_next, 100 + i, uncount_fp), i).second);
    }
    if (mode != overflow::none) {
        // homed in a group that is full, so these land in the next one and count themselves in the
        // home group's counter on the way
        for (std::uint64_t i = 0; i < uncount_overflowers; ++i) {
            REQUIRE(map.try_emplace(key(uncount_home, 200 + i, uncount_fp), i).second);
        }
    }
    if (mode == overflow::erased) {
        for (std::uint64_t i = 0; i < uncount_overflowers; ++i) {
            REQUIRE(map.erase(key(uncount_home, 200 + i, uncount_fp)) == 1U);
        }
    }
    return map;
}

} // namespace

TEST_CASE("erase_takes_its_entry_back_out_of_the_overflow_counters") {
    auto const fresh = build(overflow::none);
    auto const churned = build(overflow::erased);
    REQUIRE(fresh.size() == churned.size());
    REQUIRE(fresh.bucket_count() == churned.bucket_count());
    for (auto const& kv : fresh) {
        REQUIRE(churned.contains(kv.first));
    }

    auto const key = uncount_keys(fresh);
    auto const absent = key(uncount_home, 999, uncount_fp);

    // The home group is full of this fingerprint, so a miss compares against all of it and then
    // has to stop -- every entry that once went past this group has been erased again.
    auto const on_fresh = comparisons_for(fresh, absent);
    REQUIRE(on_fresh == uncount_slots);
    REQUIRE(comparisons_for(churned, absent) == on_fresh);
}

// The counters are rebuilt from scratch by every rehash, through a second copy of the increment in
// fill_buckets_from_values(), and that copy needs the same check. Negating it there survives the
// rest of the suite for the same reason: a counter that comes out too high only lengthens a walk.
//
// rehash() refills even when the bucket count does not change -- a table whose probes have drifted
// under churn is the same size as a fresh one, so returning early would make it a no-op in exactly
// the case a caller reaches for it. That is what makes this measurable: the same table, the same
// slots, the counters built the other way.
TEST_CASE("a_rehash_rebuilds_the_overflow_counters_as_they_were") {
    // A table that still holds its overflow, so the home group's counter is genuinely nonzero and
    // a miss walks into the next group and stops there.
    auto map = build(overflow::live);
    auto const key = uncount_keys(map);
    auto const absent = key(uncount_home, 999, uncount_fp);

    auto const before = comparisons_for(map, absent);
    // the home group, then the next one, then a counter that says nothing went further
    REQUIRE(before == uncount_slots + uncount_slots / 2 + uncount_overflowers);

    auto const buckets = map.bucket_count();
    map.rehash(1000); // the size it already has, so only the counters are built anew
    REQUIRE(map.bucket_count() == buckets);
    REQUIRE(comparisons_for(map, absent) == before);
}

// The two halves together. A rehash builds the counters up and an erase takes them back down, and
// the second only undoes the first if both count the same way -- negating the rehash's increment
// leaves counters that are nonzero rather than zero after the erase, which neither half catches on
// its own because the probe only ever asks whether a counter is zero. Building up wrongly and
// tearing down wrongly cancel until an erase makes the difference visible.
TEST_CASE("counters_built_by_a_rehash_still_come_back_down_on_erase") {
    auto map = build(overflow::live);
    map.rehash(1000); // counters discarded and built again, at the same size
    auto const key = uncount_keys(map);
    for (std::uint64_t i = 0; i < uncount_overflowers; ++i) {
        REQUIRE(map.erase(key(uncount_home, 200 + i, uncount_fp)) == 1U);
    }

    // what is left is what `overflow::none` builds directly, so it has to cost the same
    auto const fresh = build(overflow::none);
    REQUIRE(map.size() == fresh.size());
    auto const absent = key(uncount_home, 999, uncount_fp);
    REQUIRE(comparisons_for(map, absent) == comparisons_for(fresh, absent));
}
