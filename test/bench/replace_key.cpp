#include <app/doctest.h>
#include <app/print.h>

#include <sys/types.h>
#include <third-party/nanobench.h>

namespace {

template <typename T>
void randomize_key(ankerl::nanobench::Rng* rng, uint32_t n, T* key) {
    // we limit ourselves to 32bit n
    *key = static_cast<T>(rng->bounded(n));
}

void randomize_key(ankerl::nanobench::Rng* rng, uint32_t n, std::string* key) {
    uint64_t k{};
    randomize_key(rng, n, &k);
    std::memcpy(key->data(), &k, sizeof(k));
}

auto create_initial_map(ankerl::nanobench::Rng& rng, uint32_t max_entries, uint32_t bound)
    -> ankerl::unordered_dense::map<uint32_t, uint32_t> {
    auto map = ankerl::unordered_dense::map<uint32_t, uint32_t>();
    uint32_t i = 0;
    while (map.size() < max_entries) {
        map[rng.bounded(bound)] = i++;
    }
    return map;
}

auto create_initial_map_string(ankerl::nanobench::Rng& rng, std::string prototype_key, uint32_t max_entries, uint32_t bound)
    -> ankerl::unordered_dense::map<std::string, uint32_t> {
    auto map = ankerl::unordered_dense::map<std::string, uint32_t>();
    uint32_t i = 0;

    while (map.size() < max_entries) {
        randomize_key(&rng, bound, &prototype_key);
        map[prototype_key] = i++;
    }
    return map;
}

} // namespace

TEST_CASE("bench_replace_key" * doctest::test_suite("bench") * doctest::skip()) {
    using namespace std::chrono_literals;

    uint64_t const seed = 123;
    uint32_t const max_entries_mask = 4096 - 1;
    uint32_t const bound_mask = ((max_entries_mask + 1) * 4) - 1;

    auto const min_epoch_time = 100ms;

    // using replace_key, should be fast
    auto rng = ankerl::nanobench::Rng(seed);
    auto map_a = create_initial_map(rng, max_entries_mask + 1, bound_mask);

    auto const map_size = static_cast<uint32_t>(map_a.size());
    size_t num_replaces_a = 0;
    size_t num_iters_a = 0;
    ankerl::nanobench::Bench().minEpochTime(min_epoch_time).run("replace_key", [&] {
        ++num_iters_a;
        auto const rand_num = rng();
        auto const it_offset = static_cast<decltype(map_a)::difference_type>(rand_num & max_entries_mask);
        auto const replacement_key = static_cast<uint32_t>((rand_num >> 32U) & bound_mask);
        auto const it = map_a.begin() + it_offset;

        if (map_a.replace_key(it, replacement_key).second) {
            ++num_replaces_a;
        }
    });
    REQUIRE(map_a.size() == map_size);

    // without replace_key, should be slower
    rng = ankerl::nanobench::Rng(seed);
    auto map_b = create_initial_map(rng, max_entries_mask + 1, bound_mask);
    REQUIRE(map_b.size() == map_size);

    size_t num_replaces_b = 0;
    size_t num_iters_b = 0;
    ankerl::nanobench::Bench().minEpochTime(min_epoch_time).run("erase & try_emplace", [&] {
        ++num_iters_b;
        auto const rand_num = rng();
        auto const it_offset = static_cast<decltype(map_b)::difference_type>(rand_num & max_entries_mask);
        auto const replacement_key = static_cast<uint32_t>((rand_num >> 32U) & bound_mask);

        auto const it = map_b.begin() + it_offset;
        if (!map_b.contains(replacement_key)) {
            ++num_replaces_b;
            auto const old_value = it->second;
            map_b.erase(it);
            map_b.try_emplace(replacement_key, old_value);
        }
    });

    test::print("iters:          {:10} {:10}\n", num_iters_a, num_iters_b);
    test::print("replaces/iters: {:10.3f} {:10.3f}\n",
                static_cast<double>(num_replaces_a) / static_cast<double>(num_iters_a),
                static_cast<double>(num_replaces_b) / static_cast<double>(num_iters_b));

    // can't compare maps for equality because the order in the maps are different, so the auto const it = map_b.begin() +
    // it_offset; does not point to the same element in both benchmarks.
}

TEST_CASE("bench_replace_key_string" * doctest::test_suite("bench") * doctest::skip()) {
    using namespace std::chrono_literals;

    uint64_t const seed = 123;
    uint32_t const max_entries_mask = 4096 - 1;
    uint32_t const bound_mask = ((max_entries_mask + 1) * 4) - 1;

    auto const min_epoch_time = 100ms;

    // using replace_key, should be fast
    auto rng = ankerl::nanobench::Rng(seed);
    auto prototype_key = std::string(200, 'x');
    auto map_a = create_initial_map_string(rng, prototype_key, max_entries_mask + 1, bound_mask);

    auto const map_size = static_cast<uint32_t>(map_a.size());
    size_t num_replaces_a = 0;
    size_t num_iters_a = 0;
    ankerl::nanobench::Bench().minEpochTime(min_epoch_time).run("replace_key", [&] {
        ++num_iters_a;
        auto const rand_num = rng();
        auto const it_offset = static_cast<decltype(map_a)::difference_type>(rand_num & max_entries_mask);

        auto const it = map_a.begin() + it_offset;
        randomize_key(&rng, bound_mask, &prototype_key);
        if (map_a.replace_key(it, prototype_key).second) {
            ++num_replaces_a;
        }
    });
    REQUIRE(map_a.size() == map_size);

    // without replace_key, should be slower
    rng = ankerl::nanobench::Rng(seed);
    auto map_b = create_initial_map_string(rng, prototype_key, max_entries_mask + 1, bound_mask);
    REQUIRE(map_b.size() == map_size);

    size_t num_replaces_b = 0;
    size_t num_iters_b = 0;
    ankerl::nanobench::Bench().minEpochTime(min_epoch_time).run("erase & try_emplace", [&] {
        ++num_iters_b;
        auto const rand_num = rng();
        auto const it_offset = static_cast<decltype(map_b)::difference_type>(rand_num & max_entries_mask);

        auto const it = map_b.begin() + it_offset;
        randomize_key(&rng, bound_mask, &prototype_key);

        if (!map_b.contains(prototype_key)) {
            ++num_replaces_b;
            auto const old_value = it->second;
            map_b.erase(it);
            map_b.try_emplace(prototype_key, old_value);
        }
    });

    test::print("iters:          {:10} {:10}\n", num_iters_a, num_iters_b);
    test::print("replaces/iters: {:10.3f} {:10.3f}\n",
                static_cast<double>(num_replaces_a) / static_cast<double>(num_iters_a),
                static_cast<double>(num_replaces_b) / static_cast<double>(num_iters_b));

    // can't compare maps for equality because the order in the maps are different, so the auto const it = map_b.begin() +
    // it_offset; does not point to the same element in both benchmarks.
}