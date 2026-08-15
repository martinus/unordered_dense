#include <ankerl/unordered_dense.h>

#include <app/counter.h>
#include <app/doctest.h>

#include <fmt/format.h>

TEST_CASE_MAP("extract", counter::obj, counter::obj) {
    auto counts = counter();
    INFO(counts);

    auto container = typename map_t::value_container_type();
    {
        auto map = map_t();

        for (size_t i = 0; i < 100; ++i) {
            map.try_emplace(counter::obj{i, counts}, i, counts);
        }

        container = std::move(map).extract();
    }

    REQUIRE(container.size() == 100U);
    for (size_t i = 0; i < container.size(); ++i) {
        REQUIRE(container[i].first.get() == i);
        REQUIRE(container[i].second.get() == i);
    }
}

// extract() documents that *this is emptied, and size() and find() agree with that, so a map that has been extracted
// from has to actually be usable again. It wasn't: the buckets still indexed into the container that had just left.
TEST_CASE_MAP("extract_leaves_a_usable_map", int, int) {
    auto map = map_t();
    for (int i = 0; i < 100; ++i) {
        map[i] = i;
    }

    auto container = std::move(map).extract();
    REQUIRE(container.size() == 100U);
    REQUIRE(map.empty());
    REQUIRE(map.size() == 0U);
    REQUIRE(map.find(0) == map.end());

    // refill it with keys that hash to the buckets the extracted elements used
    for (int i = 0; i < 200; ++i) {
        map[i] = i + 1;
    }
    REQUIRE(map.size() == 200U);
    for (int i = 0; i < 200; ++i) {
        auto it = map.find(i);
        REQUIRE(it != map.end());
        REQUIRE(it->second == i + 1);
    }
}

TEST_CASE_MAP("extract_element", counter::obj, counter::obj) {
    auto counts = counter();
    INFO(counts);

    counts("init");
    auto map = map_t();
    for (size_t i = 0; i < 100; ++i) {
        map.try_emplace(counter::obj{i, counts}, i, counts);
    }

    // extract(key)
    for (size_t i = 0; i < 20; ++i) {
        auto query = counter::obj{i, counts};
        counts("before remove 1");
        auto opt = map.extract(query);
        counts("after remove 1");
        REQUIRE(opt);
        REQUIRE(opt->first.get() == i);
        REQUIRE(opt->second.get() == i);
    }
    REQUIRE(map.size() == 80);

    // extract iterator
    for (size_t i = 20; i < 100; ++i) {
        auto query = counter::obj{i, counts};
        auto it = map.find(query);
        REQUIRE(it != map.end());
        auto opt = map.extract(it);
        REQUIRE(opt.first.get() == i);
        REQUIRE(opt.second.get() == i);
    }
    REQUIRE(map.empty());
}

// extract(iterator) has to find the bucket that points at the element before it can erase it, and
// it does that by walking the probe sequence from the key's home bucket. A deletion sweep removed
// the statement that advances that walk -- `bucket_idx = next(bucket_idx)` -- and nothing failed,
// which can only mean the loop never went round: every element every test extracted was already
// sitting in its home bucket, so the first look was always the right one.
//
// A hash that sends every key to the same bucket makes the walk unavoidable, and extracting from
// the middle makes it several steps long. Without the advance this hangs rather than fails, which
// is a verdict the mutation tool reports as `hang` and is just as dead.
namespace {

struct always_collides {
    using is_avalanching = void;

    auto operator()(int /*unused*/) const noexcept -> uint64_t {
        return 0;
    }
};

} // namespace

TEST_CASE("extract_by_iterator_walks_the_probe_sequence") {
    auto map = ankerl::unordered_dense::map<int, int, always_collides>();
    for (int i = 0; i < 10; ++i) {
        map.try_emplace(i, i * 10);
    }

    auto const it = map.begin() + 5;
    auto const key = it->first;
    auto const value = it->second;

    auto const extracted = map.extract(it);
    REQUIRE(extracted.first == key);
    REQUIRE(extracted.second == value);
    REQUIRE(map.size() == 9);
    REQUIRE(map.find(key) == map.end());

    // and every other key is still reachable, which is what says the walk stopped at the right
    // bucket rather than at some other element's
    for (int i = 0; i < 10; ++i) {
        if (i == key) {
            continue;
        }
        auto found = map.find(i);
        REQUIRE(found != map.end());
        REQUIRE(found->second == i * 10);
    }
}
