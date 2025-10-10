#include <app/doctest.h>
#include <app/print.h>

#include <third-party/nanobench.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// These tests were all automatically created with Claude Sonnet 4.5
// reviewed by Martin Leitner-Ankerl

TEST_CASE_MAP("replace_key_basic", int, int) {
    auto map = map_t();
    map[1] = 100;
    map[2] = 200;
    map[3] = 300;

    auto it = map.find(2);
    REQUIRE(it != map.end());
    REQUIRE(it->first == 2);
    REQUIRE(it->second == 200);

    // Update key 2 to 5
    auto [new_it, success] = map.replace_key(it, 5);
    REQUIRE(success);
    REQUIRE(new_it == it); // Same iterator
    REQUIRE(new_it->first == 5);
    REQUIRE(new_it->second == 200); // Value unchanged
    REQUIRE(map.size() == 3);

    // Verify old key is gone and new key exists
    REQUIRE(map.find(2) == map.end());
    REQUIRE(map.find(5) != map.end());
    REQUIRE(map[5] == 200);
}

TEST_CASE_MAP("replace_key_duplicate_fails", int, int) {
    auto map = map_t();
    map[1] = 100;
    map[2] = 200;
    map[3] = 300;

    auto it = map.find(2);
    REQUIRE(it != map.end());

    // Try to update key 2 to 3 (which already exists)
    auto [new_it, success] = map.replace_key(it, 3);
    REQUIRE_FALSE(success);
    REQUIRE(new_it == map.find(3)); // Returns iterator to existing key
    REQUIRE(new_it->second == 300); // Points to the existing element

    // Original element should be unchanged
    auto it2 = map.find(2);
    REQUIRE(it2 != map.end());
    REQUIRE(it2->second == 200);
    REQUIRE(map.size() == 3);
}

TEST_CASE_MAP("replace_key_iterator_stability", int, int) {
    auto map = map_t();
    map[1] = 100;
    map[2] = 200;
    map[3] = 300;

    // Get iterators to all elements
    auto it1 = map.find(1);
    auto it2 = map.find(2);
    auto it3 = map.find(3);

    // Update key 2 to 5
    auto [new_it, success] = map.replace_key(it2, 5);
    REQUIRE(success);

    // All original iterators should still be valid
    REQUIRE(it1->first == 1);
    REQUIRE(it1->second == 100);
    REQUIRE(it2->first == 5); // Updated key
    REQUIRE(it2->second == 200);
    REQUIRE(it3->first == 3);
    REQUIRE(it3->second == 300);

    // new_it should be same as it2
    REQUIRE(new_it == it2);
}

TEST_CASE_MAP("replace_key_references_stability", int, int) {
    auto map = map_t();
    map[1] = 100;
    map[2] = 200;
    map[3] = 300;

    // Get references to values
    auto& val1 = map[1];
    auto& val2 = map[2];
    auto& val3 = map[3];

    auto it2 = map.find(2);
    auto [new_it, success] = map.replace_key(it2, 5);
    REQUIRE(success);

    // All references should still be valid
    REQUIRE(val1 == 100);
    REQUIRE(val2 == 200);
    REQUIRE(val3 == 300);

    // Modifying through old reference should work
    val2 = 250;
    REQUIRE(map[5] == 250);
}

TEST_CASE_MAP("replace_key_single_element", int, int) {
    auto map = map_t();
    map[1] = 100;

    auto it = map.find(1);
    auto [new_it, success] = map.replace_key(it, 10);
    REQUIRE(success);
    REQUIRE(new_it->first == 10);
    REQUIRE(new_it->second == 100);
    REQUIRE(map.size() == 1);
    REQUIRE(map.find(1) == map.end());
    REQUIRE(map.find(10) != map.end());
}

TEST_CASE_MAP("replace_key_strings", std::string, std::string) {
    auto map = map_t();
    map["foo"] = "bar";
    map["hello"] = "world";
    map["test"] = "value";

    auto it = map.find("hello");
    REQUIRE(it != map.end());

    auto [new_it, success] = map.replace_key(it, "goodbye");
    REQUIRE(success);
    REQUIRE(new_it->first == "goodbye");
    REQUIRE(new_it->second == "world");
    REQUIRE(map.size() == 3);

    REQUIRE(map.find("hello") == map.end());
    REQUIRE(map.find("goodbye") != map.end());
    REQUIRE(map["goodbye"] == "world");
}

TEST_CASE_MAP("replace_key_move_semantics", std::string, int) {
    auto map = map_t();
    map["key1"] = 1;
    map["key2"] = 2;

    auto it = map.find("key1");
    std::string new_key = "moved_key";

    auto [new_it, success] = map.replace_key(it, std::move(new_key));
    REQUIRE(success);
    REQUIRE(new_it->first == "moved_key");
    REQUIRE(new_it->second == 1);
    REQUIRE(map.size() == 2);
}

TEST_CASE_MAP("replace_key_same_key", int, int) {
    auto map = map_t();
    map[1] = 100;
    map[2] = 200;

    auto it = map.find(1);
    // Try to update key to itself
    auto [new_it, success] = map.replace_key(it, 1);

    // This should fail because key 1 already exists
    REQUIRE_FALSE(success);
    REQUIRE(new_it == map.find(1));
    REQUIRE(map.size() == 2);
}

TEST_CASE_MAP("replace_key_multiple_updates", int, int) {
    auto map = map_t();
    map[1] = 100;

    auto it = map.find(1);

    // First update: 1 -> 2
    auto [it1, s1] = map.replace_key(it, 2);
    REQUIRE(s1);
    REQUIRE(it1->first == 2);

    // Second update: 2 -> 3
    auto [it2, s2] = map.replace_key(it1, 3);
    REQUIRE(s2);
    REQUIRE(it2->first == 3);

    // Third update: 3 -> 4
    auto [it3, s3] = map.replace_key(it2, 4);
    REQUIRE(s3);
    REQUIRE(it3->first == 4);
    REQUIRE(it3->second == 100);

    REQUIRE(map.size() == 1);
    REQUIRE(map.find(1) == map.end());
    REQUIRE(map.find(2) == map.end());
    REQUIRE(map.find(3) == map.end());
    REQUIRE(map[4] == 100);
}

TEST_CASE_MAP("replace_key_large_map", int, int) {
    auto map = map_t();

    // Insert many elements
    for (int i = 0; i < 1000; ++i) {
        map[i] = i * 10;
    }

    // Update middle element
    auto it = map.find(500);
    REQUIRE(it != map.end());

    auto [new_it, success] = map.replace_key(it, 10000);
    REQUIRE(success);
    REQUIRE(new_it->first == 10000);
    REQUIRE(new_it->second == 5000);
    REQUIRE(map.size() == 1000);

    REQUIRE(map.find(500) == map.end());
    REQUIRE(map.find(10000) != map.end());

    // Verify all other elements are still accessible
    for (int i = 0; i < 1000; ++i) {
        if (i != 500) {
            REQUIRE(map.find(i) != map.end());
            REQUIRE(map[i] == i * 10);
        }
    }
}

TEST_CASE_MAP("replace_key_begin_iterator", int, int) {
    auto map = map_t();
    map[1] = 100;
    map[2] = 200;
    map[3] = 300;

    auto it = map.begin();
    int const old_key = it->first;
    int value = it->second;

    auto [new_it, success] = map.replace_key(it, 999);
    REQUIRE(success);
    REQUIRE(new_it->first == 999);
    REQUIRE(new_it->second == value);
    REQUIRE(map.size() == 3);
    REQUIRE(map.find(old_key) == map.end());
    REQUIRE(map.find(999) != map.end());
}

TEST_CASE_MAP("replace_key_end_minus_one", int, int) {
    auto map = map_t();
    map[1] = 100;
    map[2] = 200;
    map[3] = 300;

    // Get last element (order is implementation-dependent, but we can get it)
    auto it = map.begin();
    std::advance(it, map.size() - 1);

    int const old_key = it->first;
    int value = it->second;

    auto [new_it, success] = map.replace_key(it, 888);
    REQUIRE(success);
    REQUIRE(new_it->first == 888);
    REQUIRE(new_it->second == value);
    REQUIRE(map.size() == 3);
    REQUIRE(map.find(old_key) == map.end());
}

TEST_CASE_MAP("replace_key_collision_chain", int, int) {
    auto map = map_t();

    // Insert elements that might collide
    for (int i = 0; i < 100; ++i) {
        map[i] = i * 2;
    }

    // Update an element in the middle
    auto it = map.find(50);
    REQUIRE(it != map.end());

    auto [new_it, success] = map.replace_key(it, 5000);
    REQUIRE(success);
    REQUIRE(new_it->first == 5000);
    REQUIRE(new_it->second == 100);

    // Verify all elements still accessible
    for (int i = 0; i < 100; ++i) {
        if (i == 50) {
            REQUIRE(map.find(i) == map.end());
        } else {
            REQUIRE(map.find(i) != map.end());
            REQUIRE(map[i] == i * 2);
        }
    }
    REQUIRE(map[5000] == 100);
}

TEST_CASE_MAP("replace_key_after_rehash", int, int) {
    auto map = map_t();

    // Insert elements to trigger potential rehash
    for (int i = 0; i < 10; ++i) {
        map[i] = i;
    }

    map.reserve(1000); // Force rehash

    auto it = map.find(5);
    REQUIRE(it != map.end());

    auto [new_it, success] = map.replace_key(it, 555);
    REQUIRE(success);
    REQUIRE(new_it->first == 555);
    REQUIRE(new_it->second == 5);
    REQUIRE(map.find(5) == map.end());
    REQUIRE(map.find(555) != map.end());
}

TEST_CASE_MAP("replace_key_preserve_value_modifications", int, int) {
    auto map = map_t();
    map[1] = 100;
    map[2] = 200;

    auto it = map.find(1);
    it->second = 999; // Modify value before updating key

    auto [new_it, success] = map.replace_key(it, 10);
    REQUIRE(success);
    REQUIRE(new_it->first == 10);
    REQUIRE(new_it->second == 999); // Modified value should be preserved
}

TEST_CASE_MAP("replace_key_duplicate_with_different_value", int, int) {
    auto map = map_t();
    map[1] = 100;
    map[2] = 200;

    auto it = map.find(1);

    // Try to update to existing key 2
    auto [new_it, success] = map.replace_key(it, 2);
    REQUIRE_FALSE(success);
    REQUIRE(new_it == map.find(2));
    REQUIRE(new_it->first == 2);
    REQUIRE(new_it->second == 200); // Returns the existing element's value

    // Original element unchanged
    auto orig_it = map.find(1);
    REQUIRE(orig_it != map.end());
    REQUIRE(orig_it->second == 100);
}

TEST_CASE_MAP("replace_key_stress_test", int, int) {
    auto map = map_t();

    // Build initial map
    for (int i = 0; i < 500; ++i) {
        map[i] = i * 3;
    }

    // Update every 5th element
    for (int i = 0; i < 500; i += 5) {
        auto it = map.find(i);
        if (it != map.end()) {
            int new_key = i + 5000;
            auto [new_it, success] = map.replace_key(it, new_key);
            REQUIRE(success);
            REQUIRE(new_it->first == new_key);
            REQUIRE(new_it->second == i * 3);
        }
    }

    // Verify final state
    REQUIRE(map.size() == 500);

    for (int i = 0; i < 500; ++i) {
        if (i % 5 == 0) {
            REQUIRE(map.find(i) == map.end());
            REQUIRE(map.find(i + 5000) != map.end());
            REQUIRE(map[i + 5000] == i * 3);
        } else {
            REQUIRE(map.find(i) != map.end());
            REQUIRE(map[i] == i * 3);
        }
    }
}

TEST_CASE_MAP("replace_key_all_elements_sequentially", int, int) {
    auto map = map_t();

    // Insert 20 elements
    for (int i = 0; i < 20; ++i) {
        map[i] = i * 100;
    }

    // Update all keys by adding 1000
    std::vector<std::pair<int, int>> elements;
    for (auto& [k, v] : map) {
        elements.push_back({k, v});
    }

    for (auto [old_key, value] : elements) {
        auto it = map.find(old_key);
        if (it != map.end()) {
            auto [new_it, success] = map.replace_key(it, old_key + 1000);
            REQUIRE(success);
        }
    }

    // Verify all keys are updated
    REQUIRE(map.size() == 20);
    for (int i = 0; i < 20; ++i) {
        REQUIRE(map.find(i) == map.end());
        REQUIRE(map.find(i + 1000) != map.end());
        REQUIRE(map[i + 1000] == i * 100);
    }
}

// SET TESTS - replace_key works the same for sets as for maps

TEST_CASE_SET("replace_key_set_basic", int) {
    auto set = set_t();
    set.insert(1);
    set.insert(2);
    set.insert(3);

    auto it = set.find(2);
    REQUIRE(it != set.end());
    REQUIRE(*it == 2);

    // Update key 2 to 5
    auto [new_it, success] = set.replace_key(it, 5);
    REQUIRE(success);
    REQUIRE(new_it == it); // Same iterator
    REQUIRE(*new_it == 5);
    REQUIRE(set.size() == 3);

    // Verify old key is gone and new key exists
    REQUIRE(set.find(2) == set.end());
    REQUIRE(set.find(5) != set.end());
    REQUIRE(set.contains(5));
    REQUIRE_FALSE(set.contains(2));
}

TEST_CASE_SET("replace_key_set_duplicate_fails", int) {
    auto set = set_t();
    set.insert(1);
    set.insert(2);
    set.insert(3);

    auto it = set.find(2);
    REQUIRE(it != set.end());

    // Try to update key 2 to 3 (which already exists)
    auto [new_it, success] = set.replace_key(it, 3);
    REQUIRE_FALSE(success);
    REQUIRE(new_it == set.find(3)); // Returns iterator to existing key
    REQUIRE(*new_it == 3);

    // Original element should be unchanged
    auto it2 = set.find(2);
    REQUIRE(it2 != set.end());
    REQUIRE(*it2 == 2);
    REQUIRE(set.size() == 3);
}

TEST_CASE_SET("replace_key_set_iterator_stability", int) {
    auto set = set_t();
    set.insert(1);
    set.insert(2);
    set.insert(3);

    // Get iterators to all elements
    auto it1 = set.find(1);
    auto it2 = set.find(2);
    auto it3 = set.find(3);

    // Update key 2 to 5
    auto [new_it, success] = set.replace_key(it2, 5);
    REQUIRE(success);

    // All original iterators should still be valid
    REQUIRE(*it1 == 1);
    REQUIRE(*it2 == 5); // Updated key
    REQUIRE(*it3 == 3);

    // new_it should be same as it2
    REQUIRE(new_it == it2);
}

TEST_CASE_SET("replace_key_set_strings", std::string) {
    auto set = set_t();
    set.insert("foo");
    set.insert("hello");
    set.insert("test");

    auto it = set.find("hello");
    REQUIRE(it != set.end());

    auto [new_it, success] = set.replace_key(it, "goodbye");
    REQUIRE(success);
    REQUIRE(*new_it == "goodbye");
    REQUIRE(set.size() == 3);

    REQUIRE(set.find("hello") == set.end());
    REQUIRE(set.find("goodbye") != set.end());
    REQUIRE(set.contains("goodbye"));
}

TEST_CASE_SET("replace_key_set_single_element", int) {
    auto set = set_t();
    set.insert(42);

    auto it = set.find(42);
    auto [new_it, success] = set.replace_key(it, 999);
    REQUIRE(success);
    REQUIRE(*new_it == 999);
    REQUIRE(set.size() == 1);
    REQUIRE(set.find(42) == set.end());
    REQUIRE(set.find(999) != set.end());
}

TEST_CASE_SET("replace_key_set_same_key", int) {
    auto set = set_t();
    set.insert(1);
    set.insert(2);

    auto it = set.find(1);
    // Try to update key to itself
    auto [new_it, success] = set.replace_key(it, 1);

    // This should fail because key 1 already exists
    REQUIRE_FALSE(success);
    REQUIRE(new_it == set.find(1));
    REQUIRE(set.size() == 2);
}

TEST_CASE_SET("replace_key_set_multiple_updates", int) {
    auto set = set_t();
    set.insert(1);

    auto it = set.find(1);

    // First update: 1 -> 2
    auto [it1, s1] = set.replace_key(it, 2);
    REQUIRE(s1);
    REQUIRE(*it1 == 2);

    // Second update: 2 -> 3
    auto [it2, s2] = set.replace_key(it1, 3);
    REQUIRE(s2);
    REQUIRE(*it2 == 3);

    // Third update: 3 -> 4
    auto [it3, s3] = set.replace_key(it2, 4);
    REQUIRE(s3);
    REQUIRE(*it3 == 4);

    REQUIRE(set.size() == 1);
    REQUIRE(set.find(1) == set.end());
    REQUIRE(set.find(2) == set.end());
    REQUIRE(set.find(3) == set.end());
    REQUIRE(set.contains(4));
}

TEST_CASE_SET("replace_key_set_large", int) {
    auto set = set_t();

    // Insert many elements
    for (int i = 0; i < 1000; ++i) {
        set.insert(i);
    }

    // Update middle element
    auto it = set.find(500);
    REQUIRE(it != set.end());

    auto [new_it, success] = set.replace_key(it, 10000);
    REQUIRE(success);
    REQUIRE(*new_it == 10000);
    REQUIRE(set.size() == 1000);

    REQUIRE(set.find(500) == set.end());
    REQUIRE(set.find(10000) != set.end());

    // Verify all other elements are still accessible
    for (int i = 0; i < 1000; ++i) {
        if (i != 500) {
            REQUIRE(set.contains(i));
        }
    }
}

TEST_CASE_SET("replace_key_set_begin_iterator", int) {
    auto set = set_t();
    set.insert(1);
    set.insert(2);
    set.insert(3);

    auto it = set.begin();
    int const old_value = *it;

    auto [new_it, success] = set.replace_key(it, 999);
    REQUIRE(success);
    REQUIRE(*new_it == 999);
    REQUIRE(set.size() == 3);
    REQUIRE(set.find(old_value) == set.end());
    REQUIRE(set.find(999) != set.end());
}

TEST_CASE_SET("replace_key_set_stress_test", int) {
    auto set = set_t();

    // Build initial set
    for (int i = 0; i < 500; ++i) {
        set.insert(i);
    }

    // Update every 5th element
    for (int i = 0; i < 500; i += 5) {
        auto it = set.find(i);
        if (it != set.end()) {
            int new_key = i + 5000;
            auto [new_it, success] = set.replace_key(it, new_key);
            REQUIRE(success);
            REQUIRE(*new_it == new_key);
        }
    }

    // Verify final state
    REQUIRE(set.size() == 500);

    for (int i = 0; i < 500; ++i) {
        if (i % 5 == 0) {
            REQUIRE_FALSE(set.contains(i));
            REQUIRE(set.contains(i + 5000));
        } else {
            REQUIRE(set.contains(i));
        }
    }
}

TEST_CASE_SET("replace_key_set_move_semantics", std::string) {
    auto set = set_t();
    set.insert("key1");
    set.insert("key2");

    auto it = set.find("key1");
    std::string new_key = "moved_key";

    auto [new_it, success] = set.replace_key(it, std::move(new_key));
    REQUIRE(success);
    REQUIRE(*new_it == "moved_key");
    REQUIRE(set.size() == 2);
    REQUIRE_FALSE(set.contains("key1"));
    REQUIRE(set.contains("moved_key"));
}

TEST_CASE_MAP("replace_key_random", uint32_t, uint32_t) {
    auto map = map_t();
    auto comparison_map = std::unordered_map<uint32_t, uint32_t>();
    uint32_t idx = 0;

    // inserts an element, and updates a random element in the map
    auto rng = ankerl::nanobench::Rng();
    while (idx < 10000) {
        map[idx] = idx;
        comparison_map[idx] = idx;

        ++idx;
        auto const rng_idx = rng.bounded(idx);

        auto const map_it = map.find(rng_idx);
        auto const comparison_it = comparison_map.find(rng_idx);
        if (map_it == map.end()) {
            REQUIRE(comparison_it == comparison_map.end());
            continue;
        }
        REQUIRE(comparison_it != comparison_map.end());

        // test::print("map.replace_key(it, {})\n", idx);

        auto const replacement_idx = rng.bounded(idx * 2);
        auto const [new_it, success] = map.replace_key(map_it, replacement_idx);
        // test::print(
        //     "auto [{:5}, {:5}] = map.replace_key({:5}, {:5})\n", (new_it - map.begin()), success, rng_idx,
        //     replacement_idx);
        if (success) {
            REQUIRE(comparison_map.end() == comparison_map.find(replacement_idx));
            auto const val = comparison_it->second;
            comparison_map.erase(comparison_it);
            REQUIRE(comparison_map.try_emplace(replacement_idx, val).second);
        } else {
            auto const replacement_it = comparison_map.find(replacement_idx);
            REQUIRE(replacement_it != comparison_map.end());
            // make sure both iterators hold the same element
            REQUIRE(replacement_it->first == new_it->first);
            REQUIRE(replacement_it->second == new_it->second);
        }
    }

    // now both map and comparison_map should hold the same key-value pairs
    REQUIRE(map.size() == comparison_map.size());
    for (auto const& [k, v] : comparison_map) {
        auto it = map.find(k);
        REQUIRE(it != map.end());
        REQUIRE(it->second == v);
    }
}
