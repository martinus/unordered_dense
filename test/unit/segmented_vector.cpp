#include <ankerl/unordered_dense.h>
#include <app/counter.h>
#include <app/counting_allocator.h>

#include <app/doctest.h>

#include <cstddef>
#include <third-party/nanobench.h>

#include <algorithm>
#include <deque>
#include <iterator>
#include <type_traits>

TEST_CASE("segmented_vector") {
    counter counts;
    INFO(counts);
    {
        auto vec = ankerl::unordered_dense::segmented_vector<counter::obj>();
        for (size_t i = 0; i < 1000; ++i) {
            vec.emplace_back(i, counts);
            REQUIRE(i + 1 == counts.ctor());
        }
        REQUIRE(0 == counts.move_ctor());
        REQUIRE(0 == counts.move_assign());
        counts("before dtor");
        REQUIRE(counts.data() == counter::data_t{1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    }
    counts.check_all_done();
    REQUIRE(0 == counts.move_ctor());
    counts("done");
    REQUIRE(counts.data() == counter::data_t{1000, 0, 0, 1000, 0, 0, 0, 0, 0, 0, 0, 0, 0});
}

TEST_CASE("segmented_vector_capacity") {
    counter counts;
    INFO(counts);
    auto vec =
        ankerl::unordered_dense::segmented_vector<counter::obj, std::allocator<counter::obj>, sizeof(counter::obj) * 4>();
    REQUIRE(0 == vec.capacity());
    for (size_t i = 0; i < 50; ++i) {
        REQUIRE(i == vec.size());
        vec.emplace_back(i, counts);
        REQUIRE(i + 1 == vec.size());
        REQUIRE(vec.capacity() >= vec.size());
        REQUIRE(0 == vec.capacity() % 4);
    }
}

TEST_CASE("segmented_vector_idx") {
    counter counts;
    INFO(counts);
    auto vec =
        ankerl::unordered_dense::segmented_vector<counter::obj, std::allocator<counter::obj>, sizeof(counter::obj) * 4>();
    REQUIRE(0 == vec.capacity());
    for (size_t i = 0; i < 50; ++i) {
        vec.emplace_back(i, counts);
    }

    for (size_t i = 0; i < vec.size(); ++i) {
        REQUIRE(i == vec[i].get());
    }
}

TEST_CASE("segmented_vector_iterate") {
    counter counts;
    INFO(counts);
    auto vec =
        ankerl::unordered_dense::segmented_vector<counter::obj, std::allocator<counter::obj>, sizeof(counter::obj) * 4>();
    for (size_t i = 0; i < 50; ++i) {
        auto it = vec.begin();
        auto end = vec.end();

        REQUIRE(std::distance(it, end) == static_cast<std::ptrdiff_t>(vec.size()));
        size_t j = 0;
        while (it != end) {
            REQUIRE(it->get() == j);
            ++it;
            ++j;
        }
        vec.emplace_back(i, counts);
    }
}

TEST_CASE("segmented_vector_reserve") {
    auto counts = counts_for_allocator{};
    auto vec = ankerl::unordered_dense::segmented_vector<int, counting_allocator<int>, sizeof(int) * 16>(&counts);

    REQUIRE(0 == vec.capacity());
    REQUIRE(counts.size() < 2);
    vec.reserve(1100);
    REQUIRE(counts.size() > 63);
    counts.reset();
    REQUIRE(counts.size() == 0);
    REQUIRE(1104 == vec.capacity());

    for (size_t i = 0; i < vec.capacity(); ++i) {
        vec.emplace_back(0);
    }
    REQUIRE(counts.size() == 0);
    vec.emplace_back(123);

    // 3: 2 for std::vector<T*> reallocates, 1 for the new segment
    REQUIRE(counts.size() == 3);
}

// PR #188. Growing one segment at a time used to call m_blocks.reserve(size + 1), which
// std::vector takes literally: the new capacity is exactly size + 1, so the next segment
// reallocated the pointer array again, and every segment paid for copying every pointer before
// it. Quadratic overall, and slow enough to notice from a plain insert loop once the map held a
// few million elements. The index has to keep growing geometrically, the way push_back would
// have grown it.
TEST_CASE("segmented_vector_grows_the_block_index_geometrically") {
    auto counts = counts_for_allocator{};
    auto vec = ankerl::unordered_dense::segmented_vector<int, counting_allocator<int>, sizeof(int) * 16>(&counts);

    static constexpr auto num_blocks = size_t{256};
    static constexpr auto elements_per_block = size_t{16};
    for (size_t i = 0; i < num_blocks * elements_per_block; ++i) {
        vec.emplace_back(static_cast<int>(i));
    }

    // One allocation per segment is the unavoidable part. Doubling adds an allocate/deallocate
    // pair each of the log2(num_blocks) + 1 times the index grows, and the slack covers a
    // standard library that takes an extra allocation per container (MSVC's debug iterators do).
    // The reserve(size + 1) version reallocated the index for every segment and lands near
    // 3 * num_blocks events, far past this bound.
    REQUIRE(counts.size() <= num_blocks + 64);

    for (size_t i = 0; i < vec.size(); ++i) {
        REQUIRE(vec[i] == static_cast<int>(i));
    }
}

TEST_CASE("segmented_vector_resize") {
    auto counts = counts_for_allocator{};
    auto vec = ankerl::unordered_dense::segmented_vector<int, counting_allocator<int>, sizeof(int) * 16>(&counts);

    REQUIRE(vec.size() == 0);
    REQUIRE(vec.capacity() == 0);
    REQUIRE(counts.size() < 2);

    // noop resize
    vec.resize(0);
    REQUIRE(vec.size() == 0);
    REQUIRE(vec.capacity() == 0);
    REQUIRE(counts.size() < 2);

    // size-increase resize
    vec.resize(1100);
    REQUIRE(vec.size() == 1100);
    REQUIRE(vec.capacity() == 1104);
    REQUIRE(counts.size() > 63);
    counts.reset();
    for (size_t ix = 0; ix < 1100; ++ix) {
        REQUIRE(vec[ix] == 0);
    }

    // size-decrease resize
    vec.resize(500);
    REQUIRE(vec.size() == 500);
    REQUIRE(vec.capacity() == 1104);
    REQUIRE(counts.size() == 0);

    for (size_t ix = 0; ix < 500; ++ix) {
        REQUIRE(vec[ix] == 0);
    }

    // noop resize
    vec.resize(500, 123);
    REQUIRE(vec.size() == 500);
    REQUIRE(vec.capacity() == 1104);
    REQUIRE(counts.size() == 0);

    for (size_t ix = 0; ix < 500; ++ix) {
        REQUIRE(vec[ix] == 0);
    }

    // size-increase resize (no alloc)
    vec.resize(1100, 123);
    REQUIRE(vec.size() == 1100);
    REQUIRE(vec.capacity() == 1104);
    REQUIRE(counts.size() == 0);

    for (size_t ix = 0; ix < 500; ++ix) {
        REQUIRE(vec[ix] == 0);
    }
    for (size_t ix = 500; ix < 1100; ++ix) {
        REQUIRE(vec[ix] == 123);
    }

    // size-increase resize (alloc)
    vec.resize(2000, 42);
    REQUIRE(vec.size() == 2000);
    REQUIRE(vec.capacity() == 2000);
    REQUIRE(counts.size() > 50);
    counts.reset();

    for (size_t ix = 0; ix < 500; ++ix) {
        REQUIRE(vec[ix] == 0);
    }
    for (size_t ix = 500; ix < 1100; ++ix) {
        REQUIRE(vec[ix] == 123);
    }
    for (size_t ix = 1100; ix < 2000; ++ix) {
        REQUIRE(vec[ix] == 42);
    }

    // size-decrease resize
    vec.resize(800, 99);
    REQUIRE(vec.size() == 800);
    REQUIRE(vec.capacity() == 2000);
    REQUIRE(counts.size() == 0);

    for (size_t ix = 0; ix < 500; ++ix) {
        REQUIRE(vec[ix] == 0);
    }
    for (size_t ix = 500; ix < 800; ++ix) {
        REQUIRE(vec[ix] == 123);
    }
}

TEST_CASE("segmented_vector_resize_obj") {
    auto counts = counts_for_allocator{};
    counter obj_counts;
    INFO(obj_counts);
    {
        auto vec = ankerl::unordered_dense::
            segmented_vector<counter::obj, counting_allocator<counter::obj>, sizeof(counter::obj) * 4>(&counts);

        REQUIRE(vec.size() == 0);
        REQUIRE(vec.capacity() == 0);
        REQUIRE(counts.size() < 2);

        // noop resize
        vec.resize(0);
        REQUIRE(vec.size() == 0);
        REQUIRE(vec.capacity() == 0);
        REQUIRE(counts.size() < 2);

        // size-increase resize
        vec.resize(1100);
        REQUIRE(vec.size() == 1100);
        REQUIRE(vec.capacity() == 1100);
        REQUIRE(counts.size() > 63);
        counts.reset();
        for (size_t ix = 0; ix < 1100; ++ix) {
            REQUIRE(vec[ix] == counter::obj());
        }

        // size-decrease resize
        vec.resize(500);
        REQUIRE(vec.size() == 500);
        REQUIRE(vec.capacity() == 1100);
        REQUIRE(counts.size() == 0);

        for (size_t ix = 0; ix < 500; ++ix) {
            REQUIRE(vec[ix] == counter::obj());
        }

        // noop resize
        vec.resize(500, counter::obj(123, obj_counts));
        REQUIRE(vec.size() == 500);
        REQUIRE(vec.capacity() == 1100);
        REQUIRE(counts.size() == 0);

        for (size_t ix = 0; ix < 500; ++ix) {
            REQUIRE(vec[ix] == counter::obj());
        }

        // size-increase resize (no alloc)
        vec.resize(1100, counter::obj(123, obj_counts));
        REQUIRE(vec.size() == 1100);
        REQUIRE(vec.capacity() == 1100);
        REQUIRE(counts.size() == 0);

        for (size_t ix = 0; ix < 500; ++ix) {
            REQUIRE(vec[ix] == counter::obj());
        }
        for (size_t ix = 500; ix < 1100; ++ix) {
            REQUIRE(vec[ix] == counter::obj(123, obj_counts));
        }

        // size-increase resize (alloc)
        vec.resize(2000, counter::obj(42, obj_counts));
        REQUIRE(vec.size() == 2000);
        REQUIRE(vec.capacity() == 2000);
        REQUIRE(counts.size() > 50);
        counts.reset();

        for (size_t ix = 0; ix < 500; ++ix) {
            REQUIRE(vec[ix] == counter::obj());
        }
        for (size_t ix = 500; ix < 1100; ++ix) {
            REQUIRE(vec[ix] == counter::obj(123, obj_counts));
        }
        for (size_t ix = 1100; ix < 2000; ++ix) {
            REQUIRE(vec[ix] == counter::obj(42, obj_counts));
        }

        // size-decrease resize
        vec.resize(800, counter::obj(99, obj_counts));
        REQUIRE(vec.size() == 800);
        REQUIRE(vec.capacity() == 2000);
        REQUIRE(counts.size() == 0);

        for (size_t ix = 0; ix < 500; ++ix) {
            REQUIRE(vec[ix] == counter::obj());
        }
        for (size_t ix = 500; ix < 800; ++ix) {
            REQUIRE(vec[ix] == counter::obj(123, obj_counts));
        }
    }
    obj_counts.check_all_done();
}

using vec_t = ankerl::unordered_dense::segmented_vector<counter::obj>;
static_assert(sizeof(vec_t) == sizeof(std::vector<counter::obj*>) + sizeof(size_t));

TEST_CASE("bench_segmented_vector" * doctest::test_suite("bench") * doctest::skip()) {
    static constexpr auto num_elements = size_t{21233};

    using namespace std::literals;

    ankerl::nanobench::Rng rng(123);

    auto sv = ankerl::unordered_dense::segmented_vector<size_t>();
    for (size_t i = 0; i < num_elements; ++i) {
        sv.emplace_back(i);
    }

    ankerl::nanobench::Bench().minEpochTime(100ms).batch(sv.size()).run("shuffle stable_vector", [&] {
        rng.shuffle(sv);
    });

    auto c = std::deque<size_t>();
    for (size_t i = 0; i < num_elements; ++i) {
        c.push_back(i);
    }
    ankerl::nanobench::Bench().minEpochTime(100ms).batch(sv.size()).run("shuffle std::deque", [&] {
        rng.shuffle(c);
    });

    auto v = std::vector<size_t>();
    for (size_t i = 0; i < num_elements; ++i) {
        v.push_back(i);
    }
    ankerl::nanobench::Bench().minEpochTime(100ms).batch(sv.size()).run("shuffle std::vector", [&] {
        rng.shuffle(v);
    });
}
// The iterator indexes, so every random access operation is a single step -- but it used to
// advertise forward_iterator_tag, which sent std::distance walking element by element and made
// every algorithm that requires random access ill-formed over it.
TEST_CASE("segmented_vector_iterator_is_random_access") {
    using int_vec_t = ankerl::unordered_dense::segmented_vector<int>;

    static_assert(
        std::is_same_v<std::iterator_traits<int_vec_t::iterator>::iterator_category, std::random_access_iterator_tag>);
    static_assert(
        std::is_same_v<std::iterator_traits<int_vec_t::const_iterator>::iterator_category, std::random_access_iterator_tag>);
    static_assert(
        std::is_same_v<std::iterator_traits<ankerl::unordered_dense::segmented_map<int, int>::iterator>::iterator_category,
                       std::random_access_iterator_tag>);
#if defined(__cpp_lib_concepts)
    static_assert(std::random_access_iterator<int_vec_t::iterator>);
    static_assert(std::random_access_iterator<int_vec_t::const_iterator>);
#endif

    auto vec = int_vec_t();
    for (int i = 0; i < 1000; ++i) {
        vec.emplace_back(999 - i);
    }

    // this is the operator-() answer, not a walk
    REQUIRE(std::distance(vec.begin(), vec.end()) == 1000);
    REQUIRE(std::distance(vec.begin() + 400, vec.end() - 100) == 500);

    // it[n] and n + it
    REQUIRE(vec.begin()[7] == 992);
    REQUIRE(*(3 + vec.begin()) == 996);
    REQUIRE(*(vec.begin() + 3) == 996);

    // and an algorithm that only compiles for a random access iterator
    std::sort(vec.begin(), vec.end());
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(vec[static_cast<size_t>(i)] == i);
    }
    REQUIRE(std::binary_search(vec.begin(), vec.end(), 727));
}

// The rest of the random access iterator interface. The map only ever uses ++, *, - and the
// comparisons against end(), so everything else here is reached by user code and by algorithms and
// by nothing in these tests -- post-increment could have called operator--, -= could have added,
// and all four relational operators could have been each other.
//
// The block size is four elements, so every one of these steps crosses a block boundary, which is
// where an index-based iterator is least like a pointer.
TEST_CASE("segmented_vector_iterator_operations") {
    using small_block_vec_t = ankerl::unordered_dense::segmented_vector<int, std::allocator<int>, sizeof(int) * 4>;
    auto vec = small_block_vec_t();
    for (int i = 0; i < 20; ++i) {
        vec.emplace_back(i);
    }

    auto const first = vec.begin();
    auto const fifth = vec.begin() + 5;

    // Strict, so `<` becoming `<=` is a different answer for the equal case, and becoming `>` is a
    // different answer for the unequal one.
    REQUIRE(first < fifth);
    REQUIRE(!(fifth < first));
    REQUIRE(!(first < first));

    REQUIRE(fifth > first);
    REQUIRE(!(first > fifth));
    REQUIRE(!(first > first));

    REQUIRE(first <= fifth);
    REQUIRE(first <= first);
    REQUIRE(!(fifth <= first));

    REQUIRE(fifth >= first);
    REQUIRE(first >= first);
    REQUIRE(!(first >= fifth));

    // Post-increment returns the old position and moves forward; post-decrement the other way.
    auto it = vec.begin() + 3;
    auto const before_inc = it++;
    REQUIRE(before_inc == vec.begin() + 3);
    REQUIRE(it == vec.begin() + 4);
    REQUIRE(*before_inc == 3);
    REQUIRE(*it == 4);

    auto const before_dec = it--;
    REQUIRE(before_dec == vec.begin() + 4);
    REQUIRE(it == vec.begin() + 3);
    REQUIRE(*it == 3);

    // += and -= move by the amount given, in the direction the name says.
    auto moving = vec.begin();
    moving += 11;
    REQUIRE(*moving == 11);
    moving -= 7;
    REQUIRE(*moving == 4);
    REQUIRE(moving == vec.begin() + 4);
    REQUIRE(moving - vec.begin() == 4);

    // const_iterator compares against iterator, which is the pair the map's own end() checks use.
    auto const cit = vec.cbegin() + 4;
    REQUIRE(cit == moving);
    REQUIRE(cit <= moving);
    REQUIRE(cit >= moving);
    REQUIRE(vec.cbegin() < cit);
}

TEST_CASE("segmented_vector_back_is_the_last_element") {
    using small_block_vec_t = ankerl::unordered_dense::segmented_vector<int, std::allocator<int>, sizeof(int) * 4>;
    auto vec = small_block_vec_t();

    // Every size from one element to several blocks, so back() is asked both in the middle of a
    // block and at its very first slot -- the two cases an off-by-one index lands differently in.
    for (int i = 0; i < 20; ++i) {
        vec.emplace_back(i * 1000 + 7);
        REQUIRE(vec.back() == i * 1000 + 7);
        REQUIRE(vec.back() == vec[vec.size() - 1]);
        REQUIRE(std::as_const(vec).back() == i * 1000 + 7);
    }

    while (!vec.empty()) {
        auto const expected = static_cast<int>(vec.size() - 1) * 1000 + 7;
        REQUIRE(vec.back() == expected);
        vec.pop_back();
    }
}

// resize() growing a vector that is not empty. Every existing case grows from zero, where "how many
// more do we need" is the same number whichever way it is worked out.
TEST_CASE("segmented_vector_resize_grows_a_non_empty_vector") {
    using small_block_vec_t = ankerl::unordered_dense::segmented_vector<int, std::allocator<int>, sizeof(int) * 4>;
    auto vec = small_block_vec_t();
    vec.resize(5, 1);
    REQUIRE(vec.size() == 5);

    vec.resize(23, 2);
    REQUIRE(vec.size() == 23);
    for (size_t i = 0; i < 5; ++i) {
        REQUIRE(vec[i] == 1);
    }
    for (size_t i = 5; i < 23; ++i) {
        REQUIRE(vec[i] == 2);
    }

    // ... and the value-initializing overload, from a non-empty vector as well.
    vec.resize(30);
    REQUIRE(vec.size() == 30);
    for (size_t i = 23; i < 30; ++i) {
        REQUIRE(vec[i] == 0);
    }
    REQUIRE(vec[22] == 2);
}

// shrink_to_fit has to actually hand blocks back -- a loop that never runs leaves the memory held,
// and one that runs once too often frees a block that still has elements in it. The allocator is
// what can see the first; the sanitizer legs see the second.
TEST_CASE("segmented_vector_shrink_to_fit_frees_exactly_the_empty_blocks") {
    auto counts = counts_for_allocator{};
    auto vec = ankerl::unordered_dense::segmented_vector<int, counting_allocator<int>, sizeof(int) * 4>(&counts);

    for (int i = 0; i < 100; ++i) {
        vec.emplace_back(i);
    }
    REQUIRE(counts.size() > 20);
    REQUIRE(vec.capacity() == 100);

    vec.resize(9);
    REQUIRE(vec.capacity() == 100); // shrinking the size does not hand anything back
    vec.shrink_to_fit();

    // 9 elements at 4 per block is 3 blocks, so exactly 12 slots -- capacity() is m_blocks.size()
    // times the block size, which makes it a direct count of the blocks still held. Asking the
    // allocator instead would not do: m_blocks is itself allocated through it, and its own
    // shrink_to_fit records an event whether or not a single block was freed.
    REQUIRE(vec.capacity() == 12);
    REQUIRE(vec.size() == 9);
    for (int i = 0; i < 9; ++i) {
        REQUIRE(vec[static_cast<size_t>(i)] == i);
    }
    REQUIRE(vec.back() == 8);

    // Shrinking again has nothing left to give back.
    counts.reset();
    vec.shrink_to_fit();
    REQUIRE(counts.size() == 0);
    REQUIRE(vec.capacity() == 12);
}
