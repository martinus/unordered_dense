#pragma once

#include <ankerl/unordered_dense.h>
#include <app/counter.h>

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-W#warnings"
#endif
#include <doctest.h>
#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
#    undef DOCTEST_REQUIRE
#    define DOCTEST_REQUIRE(...)  \
        do {                      \
            if (!(__VA_ARGS__)) { \
                std::abort();     \
            }                     \
        } while (0)
#endif

namespace doctest {

[[nodiscard]] auto current_test_name() -> char const*;

} // namespace doctest

#include <deque>
#include <sstream>

template <class Key,
          class T,
          class Hash = ankerl::unordered_dense::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class AllocatorOrContainer = std::deque<std::pair<Key, T>>,
          class Bucket = ankerl::unordered_dense::bucket_type::standard,
          class BucketContainer = std::deque<Bucket>>
class deque_map : public ankerl::unordered_dense::detail::
                      table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, BucketContainer, false> {
    using base_t =
        ankerl::unordered_dense::detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, BucketContainer, false>;
    using base_t::base_t;
};

template <class Key,
          class Hash = ankerl::unordered_dense::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class AllocatorOrContainer = std::deque<Key>,
          class Bucket = ankerl::unordered_dense::bucket_type::standard,
          class BucketContainer = std::deque<Bucket>>
class deque_set : public ankerl::unordered_dense::detail::
                      table<Key, void, Hash, KeyEqual, AllocatorOrContainer, Bucket, BucketContainer, false> {
    using base_t = ankerl::unordered_dense::detail::
        table<Key, void, Hash, KeyEqual, AllocatorOrContainer, Bucket, BucketContainer, false>;
    using base_t::base_t;
};

// The map and set with the group index, so that every TEST_CASE_MAP runs on it too. Classes of
// their own rather than aliases, the way deque_map is, so that they are distinct types even when a
// test names its own bucket type or bucket container: the group index then gives way to what the
// test asked for, and this is one more instantiation of that, which costs compile time and nothing
// else -- where an alias would collapse onto the plain map and doctest refuses a type twice.
template <class Bucket, class BucketContainer>
using group_bucket_for =
    std::conditional_t<std::is_same_v<Bucket, ankerl::unordered_dense::bucket_type::standard> &&
                           std::is_same_v<BucketContainer, ankerl::unordered_dense::detail::default_container_t>,
                       ankerl::unordered_dense::bucket_type::group,
                       Bucket>;

template <class Key,
          class T,
          class Hash = ankerl::unordered_dense::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class AllocatorOrContainer = std::allocator<std::pair<Key, T>>,
          class Bucket = ankerl::unordered_dense::bucket_type::standard,
          class BucketContainer = ankerl::unordered_dense::detail::default_container_t>
class group_map : public ankerl::unordered_dense::detail::table<Key,
                                                                T,
                                                                Hash,
                                                                KeyEqual,
                                                                AllocatorOrContainer,
                                                                group_bucket_for<Bucket, BucketContainer>,
                                                                BucketContainer,
                                                                false> {
    using base_t = ankerl::unordered_dense::detail::
        table<Key, T, Hash, KeyEqual, AllocatorOrContainer, group_bucket_for<Bucket, BucketContainer>, BucketContainer, false>;
    using base_t::base_t;
};

template <class Key,
          class Hash = ankerl::unordered_dense::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class AllocatorOrContainer = std::allocator<Key>,
          class Bucket = ankerl::unordered_dense::bucket_type::standard,
          class BucketContainer = ankerl::unordered_dense::detail::default_container_t>
class group_set : public ankerl::unordered_dense::detail::table<Key,
                                                                void,
                                                                Hash,
                                                                KeyEqual,
                                                                AllocatorOrContainer,
                                                                group_bucket_for<Bucket, BucketContainer>,
                                                                BucketContainer,
                                                                false> {
    using base_t = ankerl::unordered_dense::detail::table<Key,
                                                          void,
                                                          Hash,
                                                          KeyEqual,
                                                          AllocatorOrContainer,
                                                          group_bucket_for<Bucket, BucketContainer>,
                                                          BucketContainer,
                                                          false>;
    using base_t::base_t;
};

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,misc-use-anonymous-namespace)
#define TEST_CASE_MAP(name, ...)                                            \
    TEST_CASE_TEMPLATE(name,                                                \
                       map_t,                                               \
                       ankerl::unordered_dense::map<__VA_ARGS__>,           \
                       ankerl::unordered_dense::segmented_map<__VA_ARGS__>, \
                       deque_map<__VA_ARGS__>,                              \
                       group_map<__VA_ARGS__>)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define TEST_CASE_SET(name, ...)                                            \
    TEST_CASE_TEMPLATE(name,                                                \
                       set_t,                                               \
                       ankerl::unordered_dense::set<__VA_ARGS__>,           \
                       ankerl::unordered_dense::segmented_set<__VA_ARGS__>, \
                       deque_set<__VA_ARGS__>,                              \
                       group_set<__VA_ARGS__>)

#define TYPE_TO_STRING_MAP(...)                                          /*NOLINT*/ \
    TYPE_TO_STRING(ankerl::unordered_dense::map<__VA_ARGS__>);           /*NOLINT*/ \
    TYPE_TO_STRING(ankerl::unordered_dense::segmented_map<__VA_ARGS__>); /*NOLINT*/ \
    TYPE_TO_STRING(deque_map<__VA_ARGS__>);                              /*NOLINT*/ \
    TYPE_TO_STRING(group_map<__VA_ARGS__>)                               /*NOLINT*/

#define TYPE_TO_STRING_SET(...)                                          /*NOLINT*/ \
    TYPE_TO_STRING(ankerl::unordered_dense::set<__VA_ARGS__>);           /*NOLINT*/ \
    TYPE_TO_STRING(ankerl::unordered_dense::segmented_set<__VA_ARGS__>); /*NOLINT*/ \
    TYPE_TO_STRING(deque_set<__VA_ARGS__>);                              /*NOLINT*/ \
    TYPE_TO_STRING(group_set<__VA_ARGS__>)                               /*NOLINT*/

#if defined(ANKERL_UNORDERED_DENSE_PMR)

// unfortunately there's no std::experimental::pmr::deque on macos, so just skip this here

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define TEST_CASE_PMR_MAP(name, ...)                                   \
        TEST_CASE_TEMPLATE(name,                                           \
                           map_t,                                          \
                           ankerl::unordered_dense::pmr::map<__VA_ARGS__>, \
                           ankerl::unordered_dense::pmr::segmented_map<__VA_ARGS__>)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define TEST_CASE_PMR_SET(name, ...)                                   \
        TEST_CASE_TEMPLATE(name,                                           \
                           set_t,                                          \
                           ankerl::unordered_dense::pmr::set<__VA_ARGS__>, \
                           ankerl::unordered_dense::pmr::segmented_set<__VA_ARGS__>)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define TYPE_TO_STRING_PMR_MAP(...)                                                     \
        TYPE_TO_STRING(ankerl::unordered_dense::pmr::map<__VA_ARGS__>);          /*NOLINT*/ \
        TYPE_TO_STRING(ankerl::unordered_dense::pmr::segmented_map<__VA_ARGS__>) /*NOLINT*/

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define TYPE_TO_STRING_PMR_SET(...)                                                     \
        TYPE_TO_STRING(ankerl::unordered_dense::pmr::set<__VA_ARGS__>);          /*NOLINT*/ \
        TYPE_TO_STRING(ankerl::unordered_dense::pmr::segmented_set<__VA_ARGS__>) /*NOLINT*/

#endif

// adds the most important type to strings here

TYPE_TO_STRING_MAP(counter::obj, counter::obj);
TYPE_TO_STRING_MAP(int, char const*);
TYPE_TO_STRING_MAP(int, int);
TYPE_TO_STRING_MAP(int, std::string);
TYPE_TO_STRING_MAP(std::string, size_t);
TYPE_TO_STRING_MAP(std::string, std::string);
TYPE_TO_STRING_MAP(uint64_t, uint64_t);
TYPE_TO_STRING_MAP(uint32_t, int);
TYPE_TO_STRING_MAP(uint64_t, int);
TYPE_TO_STRING_SET(counter::obj);
TYPE_TO_STRING_SET(int);
TYPE_TO_STRING_SET(std::string);
TYPE_TO_STRING_SET(uint32_t);
TYPE_TO_STRING_SET(uint64_t);
