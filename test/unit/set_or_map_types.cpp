#include <ankerl/unordered_dense.h>

#include <type_traits> // for is_same_v

// There is no TEST_CASE here: the static_asserts are the test, and they run at compile time.
//
// Scoped in a namespace of its own because these are as generic as names get -- `set1_t`, `map2_t`
// -- and at file scope they are visible to every other file that shares a unity chunk with this
// one. That is not hypothetical: unordered_set.cpp declares its own local `set1_t`, and once the
// two landed in the same translation unit -Wshadow=global reported it there, against a file that
// had done nothing wrong. An anonymous namespace would not have helped, since the shadowing is of
// a *scope* rather than of a linkage.
namespace set_or_map_types {

template <typename T>
using detect_has_mapped_type = typename T::mapped_type;

using map1_t = ankerl::unordered_dense::map<int, double>;
static_assert(std::is_same_v<double, map1_t::mapped_type>);
static_assert(ankerl::unordered_dense::detail::is_detected_v<detect_has_mapped_type, map1_t>);

using map2_t = ankerl::unordered_dense::segmented_map<int, double>;
static_assert(std::is_same_v<double, map2_t::mapped_type>);
static_assert(ankerl::unordered_dense::detail::is_detected_v<detect_has_mapped_type, map2_t>);

using set1_t = ankerl::unordered_dense::set<int>;
static_assert(!ankerl::unordered_dense::detail::is_detected_v<detect_has_mapped_type, set1_t>);

using set2_t = ankerl::unordered_dense::segmented_set<int>;
static_assert(!ankerl::unordered_dense::detail::is_detected_v<detect_has_mapped_type, set2_t>);

} // namespace set_or_map_types
