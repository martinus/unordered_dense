// Fixture for the LLDB data formatter test. Every container is printed by the
// program itself, so the expected values come from the map rather than from a
// number written down beside it and left to rot.
#include <ankerl/unordered_dense.h>
#include <cstdio>
#include <string>

int main() {
    ankerl::unordered_dense::map<std::string, int> str_map;
    str_map["alpha"] = 1;
    str_map["beta"] = 2;
    str_map["gamma"] = 3;

    ankerl::unordered_dense::map<int, int> int_map;
    for (int i = 0; i < 5; ++i) {
        int_map[i * 100] = i;
    }

    ankerl::unordered_dense::set<std::string> str_set;
    str_set.insert("one");
    str_set.insert("two");

    ankerl::unordered_dense::segmented_map<int, int> seg_map;
    for (int i = 0; i < 3000; ++i) {
        seg_map[i] = i * 2;
    }

    ankerl::unordered_dense::segmented_vector<int> seg_vec;
    for (int i = 0; i < 2000; ++i) {
        seg_vec.emplace_back(i * 3);
    }

    ankerl::unordered_dense::map<std::string, int> empty_map;

#define GT(name) std::printf("GT %s %zu %zu\n", #name, name.size(), name.bucket_count())
    GT(str_map);
    GT(int_map);
    GT(str_set);
    GT(seg_map);
    GT(empty_map);
#undef GT
    std::printf("GT seg_vec %zu -\n", seg_vec.size());
    // element spot checks, including both sides of a segmented block boundary
    std::printf("ELEM seg_map 511 %d\n", seg_map[511]);
    std::printf("ELEM seg_map 512 %d\n", seg_map[512]);
    std::printf("ELEM seg_map 2999 %d\n", seg_map[2999]);
    std::printf("ELEM seg_vec 1999 %d\n", seg_vec[1999]);
    std::printf("ELEM int_map 4 %d\n", int_map[400]);
    std::fflush(stdout);
    return 0; // BREAKPOINT
}
