#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// merge(source) is the standard's operation on a container that has no nodes: every element of the
// source whose key is not here already moves over, and the rest stay behind. What can be checked
// against std::unordered_map is exactly that -- which elements ended where -- and the part the
// standard does not have to worry about is that the source is *still a map* afterwards, since every
// element taken out of the middle of it leaves a gap that something else has to close.

namespace {

auto merge_key(size_t i) -> uint64_t {
    // scrambled, because sequential small integers never collide in the top bits the index uses
    return static_cast<uint64_t>(i + 1) * UINT64_C(0x9E3779B97F4A7C15);
}

// Distinguishable per source, so "the element already here wins" is visible in the value and not
// only in the key.
auto merge_val(size_t i, int side) -> uint64_t {
    return static_cast<uint64_t>(i) * 2 + static_cast<uint64_t>(side);
}

template <typename Map>
auto merge_sorted(Map const& map) -> std::vector<std::pair<uint64_t, uint64_t>> {
    auto out = std::vector<std::pair<uint64_t, uint64_t>>(map.begin(), map.end());
    std::sort(out.begin(), out.end());
    return out;
}

// Builds the same two maps twice -- once as the map under test, once as std::unordered_map -- merges
// both, and requires that the two ended up with the same elements on both sides. Then asks the
// merged-from map the questions only a rebuilt index can answer.
template <typename Dest, typename Src = Dest>
void merge_matches_std(std::vector<size_t> const& dest_ids, std::vector<size_t> const& src_ids) {
    auto dest = Dest();
    auto src = Src();
    auto ref_dest = std::unordered_map<uint64_t, uint64_t>();
    auto ref_src = std::unordered_map<uint64_t, uint64_t>();
    for (auto const id : dest_ids) {
        dest.try_emplace(merge_key(id), merge_val(id, 1));
        ref_dest.try_emplace(merge_key(id), merge_val(id, 1));
    }
    for (auto const id : src_ids) {
        src.try_emplace(merge_key(id), merge_val(id, 0));
        ref_src.try_emplace(merge_key(id), merge_val(id, 0));
    }

    dest.merge(src);
    ref_dest.merge(ref_src);

    REQUIRE(merge_sorted(dest) == merge_sorted(ref_dest));
    REQUIRE(merge_sorted(src) == merge_sorted(ref_src));

    // The source lost elements out of the middle of its value container, so its whole index had to
    // be rebuilt. Every survivor has to be findable and every key that left has to miss -- an index
    // still pointing at where an element used to be would answer both of those wrong.
    REQUIRE(src.size() == ref_src.size());
    for (auto const& [key, value] : ref_src) {
        auto it = src.find(key);
        REQUIRE(it != src.end());
        REQUIRE(it->second == value);
    }
    for (auto const id : src_ids) {
        if (ref_src.count(merge_key(id)) == 0) {
            REQUIRE(src.find(merge_key(id)) == src.end());
        }
    }

    // and it still takes new work: an insert probes the rebuilt index, an erase unwinds it again
    auto const before = src.size();
    src.try_emplace(merge_key(1000000), 7);
    REQUIRE(src.size() == before + 1);
    REQUIRE(src.at(merge_key(1000000)) == 7U);
    REQUIRE(src.erase(merge_key(1000000)) == 1U);
    REQUIRE(src.size() == before);
    for (auto const& [key, value] : ref_src) {
        REQUIRE(src.at(key) == value);
    }
}

auto merge_ids(size_t first, size_t count) -> std::vector<size_t> {
    auto out = std::vector<size_t>();
    for (size_t i = 0; i < count; ++i) {
        out.push_back(first + i);
    }
    return out;
}

} // namespace

// Over all four container shapes TEST_CASE_MAP covers, which is not decoration here: deque_map's
// values are not contiguous, and the compaction indexes and pops them; big_map indexes with 64 bits.
TEST_CASE_MAP("merge_against_std_unordered_map", uint64_t, uint64_t) {
    // Every source length up to past two lookahead windows, at three overlaps: none, every second
    // one, and all of them. The lengths matter because the elements that stay behind are compacted
    // down over the gaps the ones that left made, and where the first gap falls decides how much of
    // that loop runs at all.
    for (size_t len = 0; len <= 40; ++len) {
        merge_matches_std<map_t>(merge_ids(1000, len), merge_ids(0, len)); // disjoint
        merge_matches_std<map_t>(merge_ids(0, len), merge_ids(0, len));    // identical
        auto every_second = std::vector<size_t>();
        for (size_t i = 0; i < len; i += 2) {
            every_second.push_back(i);
        }
        merge_matches_std<map_t>(every_second, merge_ids(0, len));
    }

    // A source that ends where the destination begins, so the first elements move and the last stay
    // -- and the other way around, so the compaction starts at the very first element.
    merge_matches_std<map_t>(merge_ids(500, 500), merge_ids(0, 1000));
    merge_matches_std<map_t>(merge_ids(0, 500), merge_ids(0, 1000));

    // Enough to grow the destination several times while the merge is running, and enough that the
    // source's rebuilt index is not a single group.
    merge_matches_std<map_t>(merge_ids(4000, 1000), merge_ids(0, 5000));
    merge_matches_std<map_t>(merge_ids(0, 5000), merge_ids(0, 5000));
    merge_matches_std<map_t>({}, merge_ids(0, 5000));
}

// The walk hashes sixteen elements ahead of the one it places, and only above a measured amount of
// index -- merge_min_index_bytes, a mebibyte, which the two maps reach together at about 105000
// elements. Everything above is below that line and never runs the ring at all: without this,
// deleting the pipelined half of the walk is a mutation no test notices.
//
// The destination is what carries the size, so the *source* can be short: a source of nought to
// thirty-three with the ring switched on is what puts an edge on the priming loop, which primes
// min(16, n), and on the refill's "is there an element sixteen further along".
TEST_CASE("merge_reaches_the_pipelined_walk") {
    // not `map_t`: TEST_CASE_MAP above declares one at namespace scope
    using piped_map = ankerl::unordered_dense::map<uint64_t, uint64_t>;

    auto const big = merge_ids(0, 110000);
    for (size_t len : {size_t{0}, size_t{1}, size_t{15}, size_t{16}, size_t{17}, size_t{33}}) {
        merge_matches_std<piped_map>(big, merge_ids(110000, len)); // nothing overlaps
        merge_matches_std<piped_map>(big, merge_ids(109990, len)); // the first few do
    }

    // and a source long enough to run the ring in its steady state, half of it already there
    merge_matches_std<piped_map>(big, merge_ids(55000, 110000));
    merge_matches_std<piped_map>(big, merge_ids(110000, 110000));
}

TEST_CASE("merge_with_an_empty_map_on_either_side") {
    auto empty = ankerl::unordered_dense::map<uint64_t, uint64_t>();
    REQUIRE(empty.bucket_count() == 0U);

    auto full = ankerl::unordered_dense::map<uint64_t, uint64_t>();
    for (size_t i = 0; i < 100; ++i) {
        full.try_emplace(merge_key(i), merge_val(i, 0));
    }

    // an empty source leaves everything alone, and must not allocate an index for a map that has
    // none
    full.merge(empty);
    REQUIRE(full.size() == 100U);
    REQUIRE(empty.empty());
    REQUIRE(empty.bucket_count() == 0U);

    // Both of them empty, which is the case that says the early exit is an exit and not just a walk
    // over nothing: a merge that reaches the walk allocates the destination's index on the way in,
    // and a default constructed map that has been merged into must still have allocated nothing.
    auto untouched = ankerl::unordered_dense::map<uint64_t, uint64_t>();
    untouched.merge(empty);
    REQUIRE(untouched.bucket_count() == 0U);
    REQUIRE(untouched.empty());

    // into a map that has never allocated: every element moves, and the destination has to allocate
    // on the way
    auto fresh = ankerl::unordered_dense::map<uint64_t, uint64_t>();
    fresh.merge(full);
    REQUIRE(fresh.size() == 100U);
    REQUIRE(full.empty());
    REQUIRE(full.begin() == full.end());
    for (size_t i = 0; i < 100; ++i) {
        REQUIRE(fresh.at(merge_key(i)) == merge_val(i, 0));
        REQUIRE(full.find(merge_key(i)) == full.end());
    }

    // the emptied source is still a map
    full.try_emplace(merge_key(0), 5);
    REQUIRE(full.size() == 1U);
    REQUIRE(full.at(merge_key(0)) == 5U);
}

TEST_CASE("merge_of_a_map_into_itself_has_no_effect") {
    auto map = ankerl::unordered_dense::map<uint64_t, uint64_t>();
    for (size_t i = 0; i < 200; ++i) {
        map.try_emplace(merge_key(i), merge_val(i, 0));
    }
    auto const before = merge_sorted(map);

    map.merge(map);
    REQUIRE(map.size() == 200U);
    REQUIRE(merge_sorted(map) == before);

    map.merge(std::move(map)); // NOLINT(clang-diagnostic-self-move,bugprone-use-after-move)
    REQUIRE(map.size() == 200U);
    REQUIRE(merge_sorted(map) == before);
}

TEST_CASE("merge_takes_an_rvalue_source") {
    auto dest = ankerl::unordered_dense::map<uint64_t, uint64_t>();
    for (size_t i = 0; i < 50; ++i) {
        dest.try_emplace(merge_key(i), merge_val(i, 1));
    }
    auto src = ankerl::unordered_dense::map<uint64_t, uint64_t>();
    for (size_t i = 25; i < 75; ++i) {
        src.try_emplace(merge_key(i), merge_val(i, 0));
    }

    dest.merge(std::move(src));
    REQUIRE(dest.size() == 75U);
    // an rvalue source is still only emptied of what moved; the standard's merge does not clear it
    REQUIRE(src.size() == 25U); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    for (size_t i = 0; i < 25; ++i) {
        REQUIRE(src.find(merge_key(i)) == src.end());
    }
    for (size_t i = 25; i < 50; ++i) {
        REQUIRE(src.at(merge_key(i)) == merge_val(i, 0)); // stayed behind, dest had it already
        REQUIRE(dest.at(merge_key(i)) == merge_val(i, 1));
    }
    for (size_t i = 50; i < 75; ++i) {
        REQUIRE(dest.at(merge_key(i)) == merge_val(i, 0));
    }
}

// The standard's overload takes a source that hashes and compares differently, and re-hashes the
// keys with the destination's hasher on the way in. Here that is a different instantiation of the
// same class template.
TEST_CASE("merge_from_a_map_hashed_differently") {
    struct odd_hash {
        auto operator()(uint64_t key) const noexcept -> uint64_t {
            return key ^ UINT64_C(0x1234567890abcdef); // not avalanching, deliberately
        }
    };

    using dest_t = ankerl::unordered_dense::map<uint64_t, uint64_t>;
    using src_t = ankerl::unordered_dense::map<uint64_t, uint64_t, odd_hash>;
    merge_matches_std<dest_t, src_t>(merge_ids(0, 300), merge_ids(200, 300));
    merge_matches_std<src_t, dest_t>(merge_ids(0, 300), merge_ids(200, 300));
}

TEST_CASE("merge_a_set_and_the_segmented_containers") {
    auto s1 = ankerl::unordered_dense::set<std::string>();
    auto s2 = ankerl::unordered_dense::set<std::string>();
    for (size_t i = 0; i < 500; ++i) {
        s1.insert("key " + std::to_string(i));
    }
    for (size_t i = 250; i < 750; ++i) {
        s2.insert("key " + std::to_string(i));
    }
    s1.merge(s2);
    REQUIRE(s1.size() == 750U);
    REQUIRE(s2.size() == 250U);
    for (size_t i = 250; i < 500; ++i) {
        REQUIRE(s2.count("key " + std::to_string(i)) == 1U);
    }
    for (size_t i = 500; i < 750; ++i) {
        REQUIRE(s2.count("key " + std::to_string(i)) == 0U);
        REQUIRE(s1.count("key " + std::to_string(i)) == 1U);
    }

    merge_matches_std<ankerl::unordered_dense::segmented_map<uint64_t, uint64_t>>(merge_ids(0, 700), merge_ids(500, 700));

    auto ss1 = ankerl::unordered_dense::segmented_set<uint64_t>();
    auto ss2 = ankerl::unordered_dense::segmented_set<uint64_t>();
    for (size_t i = 0; i < 1000; ++i) {
        ss1.insert(merge_key(i));
        ss2.insert(merge_key(i + 500));
    }
    ss1.merge(std::move(ss2));
    REQUIRE(ss1.size() == 1500U);
    REQUIRE(ss2.size() == 500U); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    for (size_t i = 500; i < 1000; ++i) {
        REQUIRE(ss2.count(merge_key(i)) == 1U);
    }
    for (size_t i = 1000; i < 1500; ++i) {
        REQUIRE(ss2.count(merge_key(i)) == 0U);
    }
}

namespace {

// A key whose move constructor throws when armed. merge() moves the source's elements into the
// destination's container, so a throw there catches the source half taken apart: some of its
// elements have gone, its index no longer describes the ones that are left, and the caller still
// owns it.
struct merge_bomb {
    static constexpr int s_moved_from = -1;

    int m_value = 0;
    static inline bool s_armed = false; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    merge_bomb() = default;

    explicit merge_bomb(int value)
        : m_value(value) {}

    merge_bomb(merge_bomb const& other) = default;

    // Marks what it moved from, which is what makes "the source kept a husk" something a test can
    // see: a real moving type leaves something behind that is not a valid key, and an int that
    // copies itself would let the map hold the element twice and look fine.
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    merge_bomb(merge_bomb&& other) noexcept(false)
        : m_value(other.m_value) {
        other.m_value = s_moved_from;
        if (s_armed) {
            throw std::runtime_error("boom");
        }
    }

    auto operator=(merge_bomb const& other) -> merge_bomb& = default;

    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    auto operator=(merge_bomb&& other) noexcept(false) -> merge_bomb& {
        m_value = other.m_value;
        return *this;
    }

    ~merge_bomb() = default;

    auto operator==(merge_bomb const& other) const -> bool {
        return m_value == other.m_value;
    }
};

struct merge_bomb_hash {
    using is_avalanching = void;
    auto operator()(merge_bomb const& b) const noexcept -> uint64_t {
        return ankerl::unordered_dense::hash<int>{}(b.m_value);
    }
};

// Everything the map says about itself has to agree: what size() claims, what iterating finds, and
// what the index can look up. A merge that gave up halfway and left the index behind fails the third
// of those while passing the first two.
template <typename Map>
void merge_check_consistent(Map const& map) {
    REQUIRE(static_cast<size_t>(std::distance(map.begin(), map.end())) == map.size());

    auto findable = size_t();
    auto distinct = std::unordered_set<int>();
    for (int i = 0; i < 200; ++i) {
        if (map.find(merge_bomb{i}) != map.end()) {
            ++findable;
            distinct.insert(i);
        }
    }
    REQUIRE(findable == map.size());
    REQUIRE(distinct.size() == map.size()); // no key survived twice, which a kept husk would do
}

} // namespace

TEST_CASE("merge_survives_a_throwing_move") {
    using bomb_map = ankerl::unordered_dense::map<merge_bomb, int, merge_bomb_hash>;

    auto dest = bomb_map();
    auto src = bomb_map();
    for (int i = 0; i < 40; ++i) {
        dest.try_emplace(merge_bomb{i}, i);
    }
    for (int i = 20; i < 80; ++i) {
        src.try_emplace(merge_bomb{i}, i);
    }

    merge_bomb::s_armed = true;
    REQUIRE_THROWS_AS(dest.merge(src), std::runtime_error);
    merge_bomb::s_armed = false;

    // The element whose move threw is half moved and belongs to nobody: the destination never
    // finished building it and the source must not keep what is left, because a moved-from key is
    // not a key. Keeping it is the mistake this checks for, and it is invisible unless a moved-from
    // bomb says so.
    REQUIRE(src.find(merge_bomb{merge_bomb::s_moved_from}) == src.end());
    REQUIRE(dest.find(merge_bomb{merge_bomb::s_moved_from}) == dest.end());

    // Both halves are still maps. How far the merge got is not fixed -- the throw lands on the first
    // element that had to move, wherever the source happens to keep it -- so what is checked is the
    // invariant, plus that the two together still hold every key exactly once.
    merge_check_consistent(dest);
    merge_check_consistent(src);
    auto lost = 0;
    for (int i = 0; i < 80; ++i) {
        auto const in_dest = dest.find(merge_bomb{i}) != dest.end();
        auto const in_src = src.find(merge_bomb{i}) != src.end();
        if (i < 20) {
            REQUIRE(in_dest);
            REQUIRE(!in_src);
        } else if (i < 40) {
            REQUIRE(in_dest);
            REQUIRE(in_src); // an element the destination already had never moves
        } else {
            // in exactly one of them, unless it is the one whose move threw, which is gone
            REQUIRE(!(in_dest && in_src));
            if (!in_dest && !in_src) {
                ++lost;
            }
        }
    }
    // and exactly one is gone, not two: the repair drops the element whose move had begun and has no
    // licence to drop the one behind it
    REQUIRE(lost == 1);

    // and both still work
    src.try_emplace(merge_bomb{123}, 123);
    dest.try_emplace(merge_bomb{124}, 124);
    merge_check_consistent(dest);
    merge_check_consistent(src);

    // finishing the job with the bomb disarmed leaves the source with what the destination had
    dest.merge(src);
    merge_check_consistent(dest);
    merge_check_consistent(src);
    for (auto const& entry : src) {
        REQUIRE(dest.find(entry.first) != dest.end());
    }
}

namespace {

// A key compare that throws on the nth call. The other half of the repair: a throw out of the *probe*
// happens before anything has been moved, so the source's element is untouched and has to stay --
// where a throw out of the placement leaves a husk that has to go. The merge cannot tell which from
// the exception, so it tracks it, and this is what says the tracking works in the direction the bomb
// above does not reach.
struct merge_bomb_equal {
    static inline int s_countdown = -1; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

    auto operator()(merge_bomb const& a, merge_bomb const& b) const -> bool {
        if (s_countdown >= 0 && s_countdown-- == 0) {
            throw std::runtime_error("compare");
        }
        return a.m_value == b.m_value;
    }
};

} // namespace

TEST_CASE("merge_survives_a_throwing_key_compare") {
    using compare_bomb_map = ankerl::unordered_dense::map<merge_bomb, int, merge_bomb_hash, merge_bomb_equal>;

    // Every count of compares up to a few, so the throw lands at different places in the walk:
    // before the first element is taken, between two takes, and after the compaction has started.
    for (int nth = 0; nth < 12; ++nth) {
        auto dest = compare_bomb_map();
        auto src = compare_bomb_map();
        for (int i = 0; i < 40; ++i) {
            dest.try_emplace(merge_bomb{i}, i);
        }
        // Alternating, so that takes and compares interleave: the nth compare then lands after n/2
        // elements have already been moved out and the compaction is running.
        for (int i = 0; i < 20; ++i) {
            src.try_emplace(merge_bomb{20 + i}, i);  // already in dest: one compare, stays
            src.try_emplace(merge_bomb{100 + i}, i); // not in dest: taken, no compare
        }

        merge_bomb_equal::s_countdown = nth;
        REQUIRE_THROWS_AS(dest.merge(src), std::runtime_error);
        merge_bomb_equal::s_countdown = -1;

        // The throw came out of a compare, so nothing was half moved and nothing may be lost: every
        // key that went in is still in one map or the other, and neither holds a moved-from one.
        auto seen = std::unordered_set<int>();
        for (auto const& entry : dest) {
            seen.insert(entry.first.m_value);
        }
        for (auto const& entry : src) {
            seen.insert(entry.first.m_value);
        }
        REQUIRE(seen.size() == 60U);
        REQUIRE(seen.count(merge_bomb::s_moved_from) == 0U);

        for (int i = 0; i < 20; ++i) {
            // the shared keys are in both, wherever the throw landed
            REQUIRE(dest.find(merge_bomb{20 + i}) != dest.end());
            REQUIRE(src.find(merge_bomb{20 + i}) != src.end());
            // and each key of the source's own is in exactly one of them
            auto const in_dest = dest.find(merge_bomb{100 + i}) != dest.end();
            auto const in_src = src.find(merge_bomb{100 + i}) != src.end();
            REQUIRE(in_dest != in_src);
        }
        merge_check_consistent(src);
    }
}
