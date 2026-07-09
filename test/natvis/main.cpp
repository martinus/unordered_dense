// natvis_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <string>

import ankerl.unordered_dense;

auto& map_1() {
    const static std::unordered_map<std::string, double> m{
        {"Hello,", 1.23},
        {"world!", 4.56},
    };
    return m;
}

auto pattern_map(int num_elem) {
    std::unordered_map<std::string, double> m{};
    for (int i = 1; i <= num_elem; ++i) {
        const auto key = std::string("key_") + std::to_string(i);
        double value = i * 1.23; 
        m.emplace(key, value);
    }
    return m;
}

int main()
{
    std::unordered_map<std::string, double> map_classic = pattern_map(130);
    ankerl::unordered_dense::map<decltype(map_classic)::key_type, decltype(map_classic)::mapped_type> map_dense{};
    ankerl::unordered_dense::segmented_map<decltype(map_classic)::key_type, decltype(map_classic)::mapped_type> map_sgm{};

    std::unordered_set<decltype(map_classic)::key_type> set_classic{};
    ankerl::unordered_dense::set<decltype(map_classic)::key_type> set_dense{};
    ankerl::unordered_dense::segmented_set<decltype(map_classic)::key_type> set_sgm{};

 
    for (const auto [key, value] : map_classic) {
        set_classic.insert(key);

        map_dense[key] = value;
        map_sgm[key] = value;

        set_dense.insert(key);
        set_sgm.insert(key);
    }
    
    // Put a breakpoint here to see debug info.
    for (auto const& [key, val] : map_classic) {
        std::cout << key << " => " << val << std::endl;
    }
}

