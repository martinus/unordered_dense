* I had the warning `-Wold-style-cast` enabled, this caused a few warnings
* The interface `void erase(const_iterator pos)` is void but I'd say it should return an iterator if possible https://en.cppreference.com/w/cpp/container/unordered_map/erase
* There's no `operator+` for iterators, `std::advance(it, 123)` doesn't work
* Heterogeneous overloads not fully implemented, that would be nice to have :) (P2363R3 https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2363r3.html)
* 