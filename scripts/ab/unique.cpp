// What a set's replace()/extract() pair is worth for the one job it is shaped for: taking the
// duplicates out of a caller's std::vector<std::string>.
//
// A dense set holds its elements in exactly that container, so `replace(std::move(v))` takes the
// caller's vector as the set's own storage, drops the duplicates in place, and `extract()` hands
// it back. No string is copied and no string is moved twice. Every other way of doing this either
// copies each unique string into the set's storage, or copies it a second time to get it out
// again, or compares strings O(n log n) times instead of hashing them once.
//
//   argv: <variant> <n> [rounds] [duplicate-percent] [min-key-length]
//
// Variants: udm_replace, udm_insert, udm_insert_move, boost, boost_move, boost_view, sort. The two
// `_move` ones hand the range constructor a std::make_move_iterator pair, which is safe here
// because do_insert_hashed probes before it constructs anything: a duplicate is found and returned
// on without the value being touched, so only the unique elements are moved out of the caller's
// vector. What that is worth depends on how many of the keys are longer than the small string
// buffer, which is what min-key-length is for -- a move of a heap string steals a pointer, a move
// of one that fits inline copies the same bytes a copy would.
//
// -DUDM_UNIQUE_U64 swaps std::string for std::uint64_t, which is how much of the duplicate-rate
// reversal is the free that removing a duplicate string costs and how much is the dedup walk
// itself: an integer has no buffer to release, so what is left is the walk. `boost_view` has no
// meaning there and is not built.
//
// Only the unique operation is inside the clock. The input vector is consumed by every variant, so
// it is rebuilt from a pool of the same strings before each round, outside the clock, and the
// median round is reported rather than the mean: a rebuild that faults carries a mean.
//
// **Every variant releases the input inside the clock**, and getting that wrong is what the first
// version of this file did. `replace()` takes the caller's vector and destroys the duplicates in it;
// a variant that builds a separate set leaves the caller holding all n elements, and destroying them
// is work the caller still has to do to reach the same state. Left outside the clock it is a free
// per duplicate charged to one variant and to none of the others, which at nine tenths duplicates
// was most of the difference between them.
//
// The keys are workloads::key_source<std::string>, 8 to 135 bytes skewed short, which is what the
// scored benchmark uses. A fixed length would make the hash's length dispatch perfectly predicted
// and put every key on the heap.
#include <ankerl/unordered_dense.h>

#include <bench/workloads.h>

#if defined(UDM_AB_HAVE_BOOST)
#    include <boost/unordered/unordered_flat_set.hpp>
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace {

#if defined(UDM_UNIQUE_U64)
using elem_t = std::uint64_t;
#else
using elem_t = std::string;
#endif
using vec_t = std::vector<elem_t>;

// The set owns the caller's vector for the length of the call and gives it back.
auto udm_replace(vec_t&& v) -> vec_t {
    auto set = ankerl::unordered_dense::set<elem_t>();
    set.replace(std::move(v));
    return std::move(set).extract();
}

// The same set built the ordinary way: every unique string is copied once into the set's vector,
// and extract() is what saves the copy back out. The caller's vector is released here rather than
// on the way out of the round, because the variants that consume it release it inside the clock.
auto udm_insert(vec_t&& v) -> vec_t {
    auto set = ankerl::unordered_dense::set<elem_t>(v.begin(), v.end());
    v = vec_t();
    return std::move(set).extract();
}

// The same, with the copy into the set's vector turned into a move. Together with extract() that is
// a unique with no string copy anywhere, which is what replace() gets by owning the vector outright.
auto udm_insert_move(vec_t&& v) -> vec_t {
    auto set = ankerl::unordered_dense::set<elem_t>(std::make_move_iterator(v.begin()), std::make_move_iterator(v.end()));
    v = vec_t();
    return std::move(set).extract();
}

#if defined(UDM_AB_HAVE_BOOST)
// What a caller writes with any set that is not dense: every unique string is copied into the set
// and copied out of it again.
auto boost_unique(vec_t&& v) -> vec_t {
    auto set = boost::unordered_flat_set<elem_t>(v.begin(), v.end());
    auto out = vec_t(set.begin(), set.end());
    v = vec_t();
    return out;
}

// The same with the elements moved in. The way out stays a copy whatever the caller does, because a
// set's elements are const -- only a dense set can hand its whole container over.
auto boost_move(vec_t&& v) -> vec_t {
    auto set = boost::unordered_flat_set<elem_t>(std::make_move_iterator(v.begin()), std::make_move_iterator(v.end()));
    auto out = vec_t(set.begin(), set.end());
    v = vec_t();
    return out;
}

// The version that copies no string at all: a set of views over the caller's vector decides which
// elements are firsts, and the vector is compacted afterwards. The compaction has to come after,
// because moving an element invalidates the view of it that the set is holding. It buys the saved
// copies with an index per unique element.
#    if !defined(UDM_UNIQUE_U64)
auto boost_view_unique(vec_t&& v) -> vec_t {
    auto seen = boost::unordered_flat_set<std::string_view>();
    seen.reserve(v.size());
    auto keep = std::vector<std::uint32_t>();
    keep.reserve(v.size());
    for (std::uint32_t i = 0; i < v.size(); ++i) {
        if (seen.insert(std::string_view(v[i])).second) {
            keep.push_back(i);
        }
    }
    for (std::uint32_t j = 0; j < keep.size(); ++j) {
        if (keep[j] != j) {
            v[j] = std::move(v[keep[j]]);
        }
    }
    v.resize(keep.size());
    return std::move(v);
}
#    endif
#endif

// No hashing, no allocation, and it reorders the result.
auto sort_unique(vec_t&& v) -> vec_t {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return std::move(v);
}

// Order-independent, so the variants that reorder can be checked against the ones that do not.
auto checksum(vec_t const& v) -> std::uint64_t {
    auto sum = std::uint64_t{0};
    for (auto const& s : v) {
        sum += ankerl::unordered_dense::hash<elem_t>{}(s);
    }
    return sum;
}

auto run(std::string const& how, vec_t&& v) -> vec_t {
    if (how == "udm_replace") {
        return udm_replace(std::move(v));
    }
    if (how == "udm_insert") {
        return udm_insert(std::move(v));
    }
    if (how == "udm_insert_move") {
        return udm_insert_move(std::move(v));
    }
    if (how == "sort") {
        return sort_unique(std::move(v));
    }
#if defined(UDM_AB_HAVE_BOOST)
    if (how == "boost") {
        return boost_unique(std::move(v));
    }
    if (how == "boost_move") {
        return boost_move(std::move(v));
    }
#    if !defined(UDM_UNIQUE_U64)
    if (how == "boost_view") {
        return boost_view_unique(std::move(v));
    }
#    endif
#endif
    std::fprintf(stderr, "unknown variant %s\n", how.c_str());
    std::exit(1);
}

} // namespace

int main(int argc, char** argv) {
    workloads::tame_allocator();
    auto const how = std::string(argc > 1 ? argv[1] : "udm_replace");
    auto const n = static_cast<std::size_t>(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 100000);
    auto const rounds = static_cast<std::size_t>(argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 11);
    auto const dup_pct = static_cast<std::size_t>(argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 50);
    auto const min_len = static_cast<std::size_t>(argc > 5 ? std::strtoull(argv[5], nullptr, 10) : 0);

    // n strings of which dup_pct% repeat one of the distinct ones, then shuffled, so the duplicates
    // are not adjacent and nothing about the order helps any variant.
    auto const distinct = n - n * dup_pct / 100;
    auto rng = ankerl::nanobench::Rng(1234);
    auto pool = vec_t();
    pool.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        auto const which = i < distinct ? i : ((rng() >> 32U) * distinct) >> 32U;
        pool.emplace_back(workloads::key_source<elem_t>::get(which * UINT64_C(0x9E3779B97F4A7C15)));
#if !defined(UDM_UNIQUE_U64)
        // Padded at the end, so what makes a key unique is untouched and only its length changes.
        if (pool.back().size() < min_len) {
            pool.back().resize(min_len, '.');
        }
#endif
    }
    rng.shuffle(pool);

    auto times = std::vector<double>();
    auto expected = std::uint64_t{0};
    auto size = std::size_t{0};
    for (std::size_t round = 0; round < rounds; ++round) {
        auto v = pool;

        auto const started = std::chrono::steady_clock::now();
        auto out = run(how, std::move(v));
        auto const took = std::chrono::steady_clock::now() - started;

        auto const sum = checksum(out);
        if (round == 0) {
            expected = sum;
            size = out.size();
        } else if (sum != expected || out.size() != size) {
            std::fprintf(stderr, "%s is not deterministic\n", how.c_str());
            return 1;
        }
        // The first round runs on a heap this process has never grown; it is the warmup.
        if (round != 0) {
            times.push_back(std::chrono::duration<double, std::nano>(took).count() / static_cast<double>(n));
        }
    }
    // The median with the spread beside it, because a single round of this workload can land 40%
    // off: an 11 round run of the million-element cell read 86.78 once where five later runs of the
    // same binary read 61.4 to 64.1. A median that does not say how wide its rounds were lets that
    // through, and it did.
    std::sort(times.begin(), times.end());
    std::printf("%8.2f ns/element  [%7.2f %7.2f]  %-16s n=%-8zu dup=%-3zu%% len>=%-4zu unique=%-8zu checksum=%016llx\n",
                times[times.size() / 2],
                times.front(),
                times.back(),
                how.c_str(),
                n,
                dup_pct,
                min_len,
                size,
                static_cast<unsigned long long>(expected));
}
