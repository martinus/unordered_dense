# ankerl::unordered_dense

A fast &amp; densely stored hashmap and hashset based on robin-hood backward shift deletion.

This is a newly implemented hashmap based on lessons learned from [robin-hood-hashing](https://github.com/martinus/robin-hood-hashing)

* Perfect iteration speed. Use an `std::vector<std::pair<Key, Value>>` (maybe customizable container) for the content that is always 100% compact.
* Use an indexing data of 8 byte where overflow is no issue:
   ```
    0 1 2 3 4 5 6 7 8
   +-+-+-+-+-+-+-+-+-+
   |dist |h|   idx   |
   +-+-+-+-+-+-+-+-+-+
   ```
    * `dist`: 3 byte offset from original bucket. 0 means empty, 1 means here, ... => 2^24-2 = 16777214 collisions possible, should be plenty
    * `h`: 1 byte hash. That's enough with high probability find the correct element without having to check the key. Very useful for e.g. large strings.
    * `dist` and `h` are treated together as a single 32bit `uint32_t`.
    * `4` byte index into the `std::vector<std::pair<Key, Value>>` dense storage. That's enough for 4294 million entries.
* with a reasonable mixer of the hash. E.g. [mumx](https://github.com/martinus/map_benchmark/blob/master/src/app/mixer.h#L43-L47)? Or just multiply with a random odd 64bit number and take the upper bits? Or both.
* C++17

# Expected Advantages / Disadvantages

## Advantages
* Very fast insertion speed
* very fast loopkups
* Perfect iteration speed
* Good reallocation behavior (the vector and indexing struct resize independently)
* A low load factor is possible without too much memory overhead, because this is only about the indexing structure which is relatively compact
* standard `std::allocators` can be used without suffering any performance issue

## Disadvantages
* Deletion speed is slow: needs another lookup for the element that is moved onto the newly empty spot. On the other hand, backward shift deletion only needs to move the indexing structure around, and not the objects.
* no `const Key` in `std::pair<Key, Value>`
* Iterators are not stable
