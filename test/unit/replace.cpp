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

// replace() hashes ahead of where it places, for everything except the last few elements. The
// lookahead is only valid because removing a duplicate disturbs exactly one position -- the last --
// so the interesting cases are all at that boundary, and the thing to check is not just "the right
// keys survive" but that the *order* is the one the plain loop produced. replace() is how a caller
// bulk-loads a container it then reads back through values(), so the order is observable.
namespace {

// What the unpipelined loop does, written out: walk forward, and on a duplicate drop the element by
// moving the last one into its place. Deliberately independent of the header.
auto replace_dedup_reference(std::vector<std::pair<uint64_t, size_t>> v) -> std::vector<std::pair<uint64_t, size_t>> {
    auto seen = std::unordered_set<uint64_t>();
    auto i = size_t{0};
    while (i != v.size()) {
        if (seen.insert(v[i].first).second) {
            ++i;
        } else {
            if (i != v.size() - 1) {
                v[i] = v.back();
            }
            v.pop_back();
        }
    }
    return v;
}

auto replace_key_at(size_t i) -> uint64_t {
    return static_cast<uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15);
}

// len elements, every n-th of which repeats element 0 -- so `every == 1` is all duplicates, and a
// len that is a multiple of `every` puts a duplicate at the very last position.
auto replace_source_of(size_t len, size_t every) -> std::vector<std::pair<uint64_t, size_t>> {
    auto source = std::vector<std::pair<uint64_t, size_t>>();
    for (size_t i = 0; i < len; ++i) {
        source.emplace_back((every != 0 && i != 0 && i % every == 0) ? uint64_t{0} : replace_key_at(i), i);
    }
    return source;
}

auto replace_apply(std::vector<std::pair<uint64_t, size_t>> const& source) -> ankerl::unordered_dense::map<uint64_t, size_t> {
    // not `map_t`: TEST_CASE_MAP above declares one at namespace scope, and the unity build puts
    // this file in the same translation unit as others that do too
    using result_map = ankerl::unordered_dense::map<uint64_t, size_t>;
    auto container = result_map::value_container_type(source.begin(), source.end());
    auto map = result_map();
    map.replace(std::move(container));
    return map;
}

// same elements in the same order, not merely the same set
void check_matches_the_plain_loop(std::vector<std::pair<uint64_t, size_t>> const& source) {
    auto const expected = replace_dedup_reference(source);
    auto const map = replace_apply(source);

    REQUIRE(map.size() == expected.size());
    auto const& got = map.values();
    for (size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(got[i].first == expected[i].first);
        REQUIRE(got[i].second == expected[i].second);
    }
}

// Enough elements that the index is past pipeline_min_index_bytes, so the pipelined loop runs. The
// gate is on the index rather than the element count, so this is not a round number: 30000 elements
// take 65536 slots, which is 352 KiB of blocks against a 256 KiB threshold.
constexpr size_t pipelined_len = 30000;

} // namespace

TEST_CASE("replace_matches_the_plain_loop_at_every_length") {
    // Every length across the lookahead window and past two of them, at several duplicate rates.
    // These are all below the gate, so this is the unpipelined loop.
    for (size_t len = 0; len <= 40; ++len) {
        for (auto const every : {size_t{0}, size_t{1}, size_t{2}, size_t{3}, size_t{7}}) {
            INFO("len=" << len << " every=" << every);
            check_matches_the_plain_loop(replace_source_of(len, every));
        }
    }
}

TEST_CASE("replace_pipelined_matches_the_plain_loop_at_every_distance_from_the_end") {
    // The boundary the lookahead has to respect is the *end* of the container, not the start: the
    // pipelined loop stops pipeline_depth + 1 short and the tail finishes unpipelined. So walk a
    // duplicate through every position within two windows of the end, which is where handing over
    // between the two loops can go wrong -- and where a duplicate moves an element the ring has
    // already hashed.
    for (size_t from_end = 0; from_end <= 40; ++from_end) {
        auto source = std::vector<std::pair<uint64_t, size_t>>();
        for (size_t i = 0; i < pipelined_len; ++i) {
            source.emplace_back(replace_key_at(i), i);
        }
        source[pipelined_len - 1 - from_end].first = replace_key_at(0);

        INFO("from_end=" << from_end);
        check_matches_the_plain_loop(source);
    }
}

TEST_CASE("replace_pipelined_over_many_elements") {
    auto source = std::vector<std::pair<uint64_t, size_t>>();
    auto rng = std::mt19937_64(1234);
    for (size_t i = 0; i < pipelined_len; ++i) {
        // a quarter of them repeat an earlier key, so the dedup path runs inside the pipelined
        // region rather than only in the tail
        auto const key = (i > 100 && rng() % 4 == 0) ? source[rng() % source.size()].first : rng();
        source.emplace_back(key, i);
    }
    check_matches_the_plain_loop(source);

    // and every surviving key is findable, which is what the index is for
    auto const map = replace_apply(source);
    for (auto const& [k, v] : replace_dedup_reference(source)) {
        auto it = map.find(k);
        REQUIRE(it != map.end());
        REQUIRE(it->second == v);
    }
}

TEST_CASE("replace_a_big_map_with_a_tiny_container") {
    // replace() keeps an index it has already grown, so after this the map has a large bucket array
    // and a handful of elements. That is the one state where the pipeline's two preconditions
    // disagree -- the index is past the gate, the container is shorter than the lookahead window --
    // and running the prologue there would read past the end of the values.
    auto source = std::vector<std::pair<uint64_t, size_t>>();
    for (size_t i = 0; i < pipelined_len; ++i) {
        source.emplace_back(replace_key_at(i), i);
    }
    auto map = replace_apply(source);
    REQUIRE(map.bucket_count() > 1000U);

    for (size_t len = 0; len <= 20; ++len) {
        auto small = decltype(map)::value_container_type();
        for (size_t i = 0; i < len; ++i) {
            small.emplace_back(replace_key_at(i), i);
        }
        map.replace(std::move(small));

        INFO("len=" << len);
        REQUIRE(map.size() == len);
        for (size_t i = 0; i < len; ++i) {
            auto it = map.find(replace_key_at(i));
            REQUIRE(it != map.end());
            REQUIRE(it->second == i);
        }
    }
}
