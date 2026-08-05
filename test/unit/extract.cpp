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
