import ankerl.unordered_dense;

#include <cassert>
#include <string>

int main() {
    ankerl::unordered_dense::map<std::string, int> m;
    m["24535"] = 4;
    assert(m.size() == 1);

    auto h_int = ankerl::unordered_dense::hash<int>();
    assert(h_int(123) != 123);

    auto h_str = ankerl::unordered_dense::hash<std::string>();
    assert(h_str("123") != 123);

    auto h_ptr = ankerl::unordered_dense::hash<int*>();
    int i = 0;
    assert(h_ptr(&i) != 0);

    static_assert(ankerl::unordered_dense::hash_is_avalanching_v<ankerl::unordered_dense::hash<std::string>>);
    ankerl::unordered_dense::
        map<std::string, int, ankerl::unordered_dense::require_avalanching<ankerl::unordered_dense::hash<std::string>>>
            required;
    required["24535"] = 4;
    assert(required.size() == 1);
}
