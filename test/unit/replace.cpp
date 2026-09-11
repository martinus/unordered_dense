#include <ankerl/unordered_dense.h>

#include <app/counter.h>
#include <app/doctest.h>

#include <cstdint>
#include <random>
#include <unordered_set>
#include <utility>
#include <vector>

TEST_CASE_MAP("replace", counter::obj, counter::obj) {
    auto counts = counter{};
    INFO(counts);

    auto container = typename map_t::value_container_type{};

    for (size_t i = 0; i < 100; ++i) {
        container.emplace_back(counter::obj{i, counts}, counter::obj{i, counts});
        container.emplace_back(counter::obj{i, counts}, counter::obj{i, counts});
    }

    for (size_t i = 0; i < 10; ++i) {
        container.emplace_back(counter::obj{i, counts}, counter::obj{i, counts});
    }

    // add some elements
    auto map = map_t();
    for (size_t i = 0; i < 10; ++i) {
        map.try_emplace(counter::obj{i, counts}, counter::obj{i, counts});
    }

    map.replace(std::move(container));

    REQUIRE(map.size() == 100U);
    for (size_t i = 0; i < 100; ++i) {
        REQUIRE(map.contains(counter::obj{i, counts}));
    }
}

// replace() now hashes ahead of where it places, for everything except the last few elements. The
// lookahead is only valid because removing a duplicate disturbs exactly one position -- the last --
// so the interesting cases are all at that boundary, and the thing to check is not just "the right
// keys survive" but that the *order* is the one the plain loop produced. replace() is how a caller
// bulk-loads a container it then reads back through values(), so the order is observable.
namespace {

// What the unpipelined loop does, written out: walk forward, and on a duplicate drop the element by
// moving the last one into its place. Deliberately independent of the header.
auto dedup_like_the_plain_loop(std::vector<std::pair<uint64_t, size_t>> v) -> std::vector<std::pair<uint64_t, size_t>> {
    auto seen = std::unordered_set<uint64_t>();
    auto i = size_t{0};
    while (i != v.size()) {
        if (seen.count(v[i].first) != 0) {
            if (i != v.size() - 1) {
                v[i] = v.back();
            }
            v.pop_back();
        } else {
            seen.insert(v[i].first);
            ++i;
        }
    }
    return v;
}

auto replaced(std::vector<std::pair<uint64_t, size_t>> const& source) -> ankerl::unordered_dense::map<uint64_t, size_t> {
    auto map = ankerl::unordered_dense::map<uint64_t, size_t>();
    auto container = ankerl::unordered_dense::map<uint64_t, size_t>::value_container_type();
    for (auto const& kv : source) {
        container.push_back(kv);
    }
    map.replace(std::move(container));
    return map;
}

} // namespace

TEST_CASE("replace_pipelined_matches_the_plain_loop_at_every_length") {
    // Every length across the lookahead window and past two of them, so the tail-only case, the
    // boundary and the steady state are all covered -- at several duplicate rates, including a
    // duplicate of the very last element, which is the one the lookahead is not allowed to see.
    for (size_t len = 0; len <= 40; ++len) {
        for (auto const every : {size_t{0}, size_t{2}, size_t{3}, size_t{7}}) {
            auto source = std::vector<std::pair<uint64_t, size_t>>();
            for (size_t i = 0; i < len; ++i) {
                // every == 0 means all distinct; otherwise every n-th element repeats element 0
                auto const key = (every != 0 && i != 0 && i % every == 0)
                                     ? uint64_t{0}
                                     : static_cast<uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15);
                source.emplace_back(key, i);
            }
            auto const expected = dedup_like_the_plain_loop(source);
            auto const map = replaced(source);

            INFO("len=" << len << " every=" << every);
            REQUIRE(map.size() == expected.size());
            // same elements in the same order, not merely the same set
            auto const& got = map.values();
            for (size_t i = 0; i < expected.size(); ++i) {
                REQUIRE(got[i].first == expected[i].first);
                REQUIRE(got[i].second == expected[i].second);
            }
        }
    }
}

TEST_CASE("replace_pipelined_over_many_elements_and_growths") {
    auto source = std::vector<std::pair<uint64_t, size_t>>();
    auto rng = std::mt19937_64(1234);
    for (size_t i = 0; i < 30000; ++i) {
        // a quarter of them repeat an earlier key, so the dedup path runs inside the pipelined
        // region rather than only in the tail
        auto const key = (i > 100 && rng() % 4 == 0) ? source[rng() % source.size()].first : rng();
        source.emplace_back(key, i);
    }
    auto const expected = dedup_like_the_plain_loop(source);
    auto const map = replaced(source);

    REQUIRE(map.size() == expected.size());
    auto const& got = map.values();
    for (size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(got[i].first == expected[i].first);
        REQUIRE(got[i].second == expected[i].second);
    }
    // and every surviving key is findable, which is what the index is for
    for (auto const& [k, v] : expected) {
        auto it = map.find(k);
        REQUIRE(it != map.end());
        REQUIRE(it->second == v);
    }
}

TEST_CASE("replace_pipelined_all_duplicates") {
    auto source = std::vector<std::pair<uint64_t, size_t>>();
    for (size_t i = 0; i < 100; ++i) {
        source.emplace_back(uint64_t{7}, i);
    }
    auto const map = replaced(source);
    REQUIRE(map.size() == 1U);
    REQUIRE(map.at(7) == 0U); // the first one survives
}
