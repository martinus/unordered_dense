import ankerl.unordered_dense;

#include <cassert>
#include <cstdint>
#include <string>
#include <type_traits>

// A module consumer has to be able to *specialize* the trait, not merely read it -- that is what
// the export is for, and it is the whole mechanism for a hash that cannot be edited.
struct unedited_hash {
    auto operator()(int x) const noexcept -> std::uint64_t {
        return static_cast<std::uint64_t>(x);
    }
};

struct untouched_hash {
    auto operator()(int x) const noexcept -> std::uint64_t {
        return static_cast<std::uint64_t>(x);
    }
};

template <>
struct ankerl::unordered_dense::hash_is_avalanching<unedited_hash> : std::true_type {};

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
    static_assert(ankerl::unordered_dense::hash_is_avalanching_v<unedited_hash>);   // via the specialization below
    static_assert(!ankerl::unordered_dense::hash_is_avalanching_v<untouched_hash>); // nothing said about it
    ankerl::unordered_dense::
        map<std::string, int, ankerl::unordered_dense::require_avalanching<ankerl::unordered_dense::hash<std::string>>>
            required;
    required["24535"] = 4;
    assert(required.size() == 1);
}
