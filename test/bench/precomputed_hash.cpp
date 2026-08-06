#include <ankerl/unordered_dense.h> // for map

#include <third-party/nanobench.h> // for Bench

#include <app/doctest.h> // for TEST_CASE, skip, test_suite
#include <fmt/format.h>  // for format

#include <cstddef> // for size_t
#include <string>  // for string, to_string
#include <vector>  // for vector

// What issue #156 asks about: a handful of keys, known up front, looked up over and over. Hashing
// them is the part that can be done once, so the longer the key the more there is to save.
namespace {

using map_t = ankerl::unordered_dense::map<std::string, size_t>;

constexpr size_t num_entries = 10000;
constexpr size_t num_queries = 100;

auto padded(std::string str, size_t key_len) -> std::string {
    str.resize(key_len, '.');
    return str;
}

auto make_map(size_t key_len) -> map_t {
    auto map = map_t();
    for (size_t i = 0; i < num_entries; ++i) {
        map[padded(std::to_string(i), key_len)] = i;
    }
    return map;
}

// Half hits and half misses, so neither path is measured on its own. The misses start with a letter
// and the entries do not, which is what makes them misses.
auto make_queries(size_t key_len) -> std::vector<std::string> {
    auto queries = std::vector<std::string>();
    for (size_t i = 0; i < num_queries; ++i) {
        auto const n = std::to_string(i * 197 % num_entries);
        queries.push_back(padded(i % 2 == 0 ? n : "x" + n, key_len));
    }
    return queries;
}

void bench(size_t key_len) {
    auto const map = make_map(key_len);
    auto const queries = make_queries(key_len);

    auto hashes = std::vector<map_t::precomputed_hash>();
    hashes.reserve(queries.size());
    for (auto const& key : queries) {
        hashes.push_back(map.hash_for(key));
    }

    // Same answers, or there is nothing to compare. Checked here rather than by comparing what the
    // two loops below accumulate, since nanobench decides on its own how many times it runs each.
    for (size_t i = 0; i < queries.size(); ++i) {
        REQUIRE((map.find(queries[i]) != map.end()) == (map.find(queries[i], hashes[i]) != map.end()));
    }

    auto b = ankerl::nanobench::Bench().batch(queries.size()).relative(true);

    auto found = size_t();
    b.run(fmt::format("find, {} byte keys", key_len), [&] {
        for (auto const& key : queries) {
            found += map.find(key) != map.end() ? 1 : 0;
        }
    });
    ankerl::nanobench::doNotOptimizeAway(found);

    b.run(fmt::format("find with precomputed hash, {} byte keys", key_len), [&] {
        for (size_t i = 0; i < queries.size(); ++i) {
            found += map.find(queries[i], hashes[i]) != map.end() ? 1 : 0;
        }
    });
    ankerl::nanobench::doNotOptimizeAway(found);
}

} // namespace

TEST_CASE("bench_precomputed_hash" * doctest::test_suite("bench") * doctest::skip()) {
    bench(8);
    bench(32);
    bench(200);
}
