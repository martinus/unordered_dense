#include <ankerl/unordered_dense.h>

#include <cstdio>
#include <string>

// Shared by both consumer projects here, the add_subdirectory one and the find_package one.
// Deliberately shallow: this checks that the CMake target carries the include directory and the
// C++17 requirement, not that the map behaves. The meson suite owns behaviour.
auto main() -> int {
    auto map = ankerl::unordered_dense::map<std::string, int>();
    for (int i = 0; i < 100; ++i) {
        map[std::to_string(i)] = i;
    }
    map.erase("7");
    if (map.size() != 99 || map.at("99") != 99 || map.contains("7")) {
        return 1;
    }

    std::printf("consumed unordered_dense %d.%d.%d through CMake\n",
                ANKERL_UNORDERED_DENSE_VERSION_MAJOR,
                ANKERL_UNORDERED_DENSE_VERSION_MINOR,
                ANKERL_UNORDERED_DENSE_VERSION_PATCH);
    return 0;
}
