* I had the warning `-Wold-style-cast` enabled, this caused a few warnings
* There's no `operator+` for iterators
* The interface `void erase(const_iterator pos)` doesn't return an iterator, but I think it should if possible https://en.cppreference.com/w/cpp/container/unordered_map/erase
* Heterogeneous Overloads not fully implemented (P2363R3 https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2363r3.html)
* 