#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>

// As a hack I just renamed create aliases for boost's unordered_flat_map so I don't have to update all other code.

namespace ankerl::unordered_dense {

template <class Key,
          class T,
          class Hash = boost::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>>
using map = boost::unordered_flat_map<Key, T, Hash, KeyEqual, Allocator>;

template <class Key, class Hash = boost::hash<Key>, class KeyEqual = std::equal_to<Key>, class Allocator = std::allocator<Key>>
using set = boost::unordered_flat_set<Key, Hash, KeyEqual, Allocator>;

template <typename T>
using hash = boost::hash<T>;

} // namespace ankerl::unordered_dense
