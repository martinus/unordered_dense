#include <ankerl/unordered_dense.h>

template <typename T>
using detect_has_mapped_type = typename T::mapped_type;

using map_t = ankerl::unordered_dense::map<int, double>;
static_assert(std::is_same_v<double, map_t::mapped_type>);
static_assert(ankerl::unordered_dense::detail::is_detected_v<detect_has_mapped_type, map_t>);

using set_t = ankerl::unordered_dense::set<int>;
static_assert(!ankerl::unordered_dense::detail::is_detected_v<detect_has_mapped_type, set_t>);
