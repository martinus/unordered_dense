#include <ankerl/unordered_dense.h>

#include <app/counter.h>
#include <app/doctest.h>

#include <array>
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

// 64 KiB of payload, carrying the same tag a plain size_t would. replace() only pipelines once the
// container plus its index is too big for cache, so the window boundary -- which is at a couple of
// dozen *elements* -- is out of reach of a sweep over small values. A mapped type this size puts
// the boundary back inside a sweep of thirty, instead of needing a hundred thousand elements in
// each of a hundred and fifty cases.
struct replace_bulky {
    size_t tag{};
    std::array<uint64_t, 8192> pad{};
};

auto replace_mapped_from(size_t tag, size_t /*unused*/) -> size_t {
    return tag;
}
auto replace_mapped_from(size_t tag, replace_bulky /*unused*/) -> replace_bulky {
    return replace_bulky{tag, {}};
}
auto replace_tag_of(size_t v) -> size_t {
    return v;
}
auto replace_tag_of(replace_bulky const& v) -> size_t {
    return v.tag;
}

// len elements, every n-th of which repeats element 0 -- so `every == 1` is all duplicates, and a
// len that is a multiple of `every` puts a duplicate at the very last position, which is the one
// the lookahead is not allowed to have seen.
auto replace_source_of(size_t len, size_t every) -> std::vector<std::pair<uint64_t, size_t>> {
    auto source = std::vector<std::pair<uint64_t, size_t>>();
    for (size_t i = 0; i < len; ++i) {
        auto const key =
            (every != 0 && i != 0 && i % every == 0) ? uint64_t{0} : static_cast<uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15);
        source.emplace_back(key, i);
    }
    return source;
}

template <typename Mapped>
auto replace_apply(std::vector<std::pair<uint64_t, size_t>> const& source) -> ankerl::unordered_dense::map<uint64_t, Mapped> {
    // not `map_t`: TEST_CASE_MAP above declares one at namespace scope, and the unity build puts
    // this file in the same translation unit as others that do too
    using result_map = ankerl::unordered_dense::map<uint64_t, Mapped>;
    auto container = typename result_map::value_container_type();
    container.reserve(source.size());
    for (auto const& [key, tag] : source) {
        container.emplace_back(key, replace_mapped_from(tag, Mapped{}));
    }
    auto map = result_map();
    map.replace(std::move(container));
    return map;
}

// same elements in the same order, not merely the same set
template <typename Mapped>
void check_matches_the_plain_loop(std::vector<std::pair<uint64_t, size_t>> const& source) {
    auto const expected = replace_dedup_reference(source);
    auto const map = replace_apply<Mapped>(source);

    REQUIRE(map.size() == expected.size());
    auto const& got = map.values();
    for (size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(got[i].first == expected[i].first);
        REQUIRE(replace_tag_of(got[i].second) == expected[i].second);
    }
}

} // namespace

TEST_CASE("replace_matches_the_plain_loop_at_every_length") {
    // Every length across the lookahead window and past two of them, so the tail-only case, the
    // boundary and the steady state are all covered, at several duplicate rates.
    for (size_t len = 0; len <= 40; ++len) {
        for (auto const every : {size_t{0}, size_t{1}, size_t{2}, size_t{3}, size_t{7}}) {
            INFO("len=" << len << " every=" << every);
            check_matches_the_plain_loop<size_t>(replace_source_of(len, every));
        }
    }
}

TEST_CASE("replace_pipelined_matches_the_plain_loop_at_every_length") {
    // The same sweep with a mapped type big enough that the cache gate lets the pipeline run: from
    // sixteen elements up these cross it, which covers the window boundary at seventeen.
    for (size_t len = 0; len <= 30; ++len) {
        for (auto const every : {size_t{0}, size_t{1}, size_t{2}, size_t{3}, size_t{7}}) {
            INFO("len=" << len << " every=" << every);
            check_matches_the_plain_loop<replace_bulky>(replace_source_of(len, every));
        }
    }
}

TEST_CASE("replace_pipelined_over_many_elements") {
    auto source = std::vector<std::pair<uint64_t, size_t>>();
    auto rng = std::mt19937_64(1234);
    for (size_t i = 0; i < 100000; ++i) {
        // a quarter of them repeat an earlier key, so the dedup path runs inside the pipelined
        // region rather than only in the tail
        auto const key = (i > 100 && rng() % 4 == 0) ? source[rng() % source.size()].first : rng();
        source.emplace_back(key, i);
    }
    check_matches_the_plain_loop<size_t>(source);

    // and every surviving key is findable, which is what the index is for
    auto const map = replace_apply<size_t>(source);
    for (auto const& [k, v] : replace_dedup_reference(source)) {
        auto it = map.find(k);
        REQUIRE(it != map.end());
        REQUIRE(it->second == v);
    }
}
