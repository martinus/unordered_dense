#pragma once

#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace test {

// Every read a read only table has, called on V, which is the table or the table const, and
// checked against source, an owning table with the same values in the same order. A member that is
// a template is only compiled for a view when something calls it on one, so a read that does not
// compile there (visit, #374; cbegin, #375) stays invisible until a user finds it. A new read member
// belongs in here. It runs on map_view and set_view (index_view.cpp) and on a table whose value
// container is the caller's own (index_container.cpp). make_key(i) is the i-th key; the first n are
// the ones that were inserted.
template <typename V, typename Table, typename MakeKey>
void require_view_reads(V& v, Table const& source, std::size_t n, MakeKey make_key) {
    using key_type = typename Table::key_type;
    constexpr auto is_map = !std::is_same_v<key_type, typename Table::value_type>;

    REQUIRE(v.size() == source.size());
    REQUIRE(v.empty() == source.empty());
    REQUIRE(v.bucket_count() == source.bucket_count());
    REQUIRE(v.load_factor() == source.load_factor());
    REQUIRE(v.max_load_factor() == source.max_load_factor());
    REQUIRE(v.max_size() == source.max_size());
    REQUIRE(v.max_bucket_count() == source.max_bucket_count());
    REQUIRE(v.index_bytes() == source.index_bytes());
    REQUIRE(v.values().size() == source.size());
    REQUIRE(v.index().size() == source.index().size());
    REQUIRE(v.verify(ankerl::unordered_dense::verify_level::full));
    REQUIRE(v == v);
    (void)v.hash_function();
    (void)v.key_eq();
    (void)v.get_allocator();

    static_assert(std::is_same_v<decltype(v.cbegin()), typename std::remove_const_t<V>::const_iterator>);
    REQUIRE(std::equal(v.cbegin(), v.cend(), source.begin(), source.end()));
    REQUIRE(std::equal(v.begin(), v.end(), source.begin(), source.end()));

    auto keys = std::vector<key_type>();
    for (std::size_t i = 0; i < n + 50; ++i) {
        keys.push_back(make_key(i));
        auto const& key = keys.back();
        auto const expected = source.find(key);
        auto const found = expected != source.end();
        // every lookup overload, with and without a precomputed hash
        auto const lookups = [&](auto const& k, auto... ph) {
            auto const it = v.find(k, ph...);
            REQUIRE((it != v.end()) == found);
            if (found) {
                REQUIRE(it - v.begin() == expected - source.begin());
            }
            REQUIRE(v.contains(k, ph...) == found);
            REQUIRE(v.count(k, ph...) == (found ? 1U : 0U));
            auto const r = v.equal_range(k, ph...);
            REQUIRE(std::distance(r.first, r.second) == (found ? 1 : 0));
            if constexpr (is_map) {
                if (found) {
                    REQUIRE(v.at(k, ph...) == expected->second);
                }
            }
        };
        lookups(key);
        lookups(key, v.hash_for(key));
        if constexpr (std::is_same_v<key_type, std::string>) {
            // the transparent overloads: no std::string is built for the lookup
            auto const sv = std::string_view(key);
            lookups(sv);
            lookups(sv, v.hash_for(sv));
        }
    }

    auto const* const first = v.values().data();
    auto const visited = v.visit(keys.begin(), keys.end(), [&](auto& e) {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(e)>>);
        REQUIRE(&e >= first);
        REQUIRE(&e < first + v.size());
    });
    REQUIRE(visited == source.size());
}

} // namespace test
