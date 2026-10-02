#include <ankerl/unordered_dense.h>

#include <app/bombing_allocator.h>
#include <app/doctest.h>
#include <fuzz/run.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Loading a table from its values and its index without rehashing (#299): index(), the owning
// constructor, map_view / set_view over caller-owned memory, view(), the owning constructor from a
// view, verify(), and index_format_id.

namespace {

namespace ud = ankerl::unordered_dense;

template <typename Key>
auto iv_make_key(std::size_t i) -> Key {
    if constexpr (std::is_same_v<Key, std::string>) {
        // 8 to 40 bytes, so both the short-string buffer and the heap are in it
        return std::string(8 + (i * 7) % 33, 'a') + std::to_string(i);
    } else {
        return static_cast<Key>(i * UINT64_C(0x9E3779B97F4A7C15));
    }
}

// Key i, mapped to i in a map.
template <typename Table>
void iv_insert(Table& t, std::size_t i) {
    if constexpr (std::is_same_v<typename Table::key_type, typename Table::value_type>) {
        t.insert(iv_make_key<typename Table::key_type>(i));
    } else {
        t.try_emplace(iv_make_key<typename Table::key_type>(i), i);
    }
}

// n keys inserted, then every third erased, so that the swap-last-into-hole path has run.
template <typename Table>
auto iv_make_table(std::size_t n, bool with_erases) -> Table {
    auto t = Table();
    for (std::size_t i = 0; i < n; ++i) {
        iv_insert(t, i);
    }
    if (with_erases) {
        for (std::size_t i = 0; i < n; i += 3) {
            t.erase(iv_make_key<typename Table::key_type>(i));
        }
    }
    return t;
}

// a agrees with b on every present key, on absent keys, on size and on iteration order
template <typename A, typename B>
void iv_require_same(A const& a, B const& b, std::size_t n) {
    REQUIRE(a.size() == b.size());
    REQUIRE(std::equal(a.begin(), a.end(), b.begin(), b.end()));
    for (std::size_t i = 0; i < n + 50; ++i) {
        auto const key = iv_make_key<typename A::key_type>(i);
        auto const ia = a.find(key);
        auto const ib = b.find(key);
        REQUIRE((ia == a.end()) == (ib == b.end()));
        if (ia != a.end()) {
            REQUIRE(ia - a.begin() == ib - b.begin());
            REQUIRE(*ia == *ib);
        }
        REQUIRE(a.contains(key) == b.contains(key));
        REQUIRE(a.count(key) == b.count(key));
        auto const ra = a.equal_range(key);
        REQUIRE(std::distance(ra.first, ra.second) == static_cast<std::ptrdiff_t>(b.count(key)));
        if constexpr (!std::is_same_v<typename A::key_type, typename A::value_type>) {
            if (ia != a.end()) {
                REQUIRE(a.at(key) == b.at(key));
            }
        }
    }
}

template <typename Table>
auto iv_copy_index(Table const& t) -> std::vector<typename Table::index_block> {
    auto const idx = t.index();
    return {idx.data(), idx.data() + idx.size()};
}

template <typename Table>
auto iv_as_view(std::vector<typename Table::index_block> const& blocks) -> typename Table::index_view {
    return {blocks.data(), blocks.size()};
}

// The slot that points at value i, as (block, lane).
template <typename Block>
auto iv_slot_of(std::vector<Block>& blocks, std::size_t value_idx) -> std::pair<std::size_t, std::size_t> {
    for (std::size_t b = 0; b < blocks.size(); ++b) {
        for (std::size_t lane = 0; lane < 16; ++lane) {
            if (blocks[b].m_fingerprints[lane] != 0 && blocks[b].m_index[lane] == value_idx) {
                return {b, lane};
            }
        }
    }
    FAIL("no slot points at value " << value_idx);
    return {};
}

template <typename Block>
void iv_swap_slots(std::vector<Block>& blocks, std::size_t value_a, std::size_t value_b) {
    auto const a = iv_slot_of(blocks, value_a);
    auto const b = iv_slot_of(blocks, value_b);
    std::swap(blocks[a.first].m_index[a.second], blocks[b.first].m_index[b.second]);
}

} // namespace

TEST_CASE_TEMPLATE("index_view_round_trip",
                   Table,
                   ud::map<std::uint64_t, std::size_t>,
                   ud::map<std::uint64_t,
                           std::size_t,
                           ud::hash<std::uint64_t>,
                           std::equal_to<std::uint64_t>,
                           std::allocator<std::pair<std::uint64_t, std::size_t>>,
                           ud::bucket_type::group_big>,
                   ud::map<std::string, std::size_t>,
                   ud::map<std::string,
                           std::size_t,
                           ud::hash<std::string>,
                           std::equal_to<std::string>,
                           std::allocator<std::pair<std::string, std::size_t>>,
                           ud::bucket_type::group_big>,
                   ud::set<std::uint64_t>,
                   ud::set<std::string>) {
    // 63, 64, 65 and 1025 values sit on the owning check's bitmap word boundaries: a bitmap a word
    // short is caught there under ASan
    for (auto const n : {std::size_t{0},
                         std::size_t{1},
                         std::size_t{15},
                         std::size_t{63},
                         std::size_t{64},
                         std::size_t{65},
                         std::size_t{100},
                         std::size_t{1025},
                         std::size_t{3000}}) {
        for (auto const with_erases : {false, true}) {
            CAPTURE(n);
            CAPTURE(with_erases);
            auto const source = iv_make_table<Table>(n, with_erases);
            REQUIRE(source.verify(ud::verify_level::full));
            REQUIRE(source.verify());

            // view(): shares the bytes
            auto const v = source.view();
            REQUIRE(v.verify(ud::verify_level::full));
            iv_require_same(v, source, n);

            // a view built by hand over the same two arrays, both trust levels
            for (auto const t : {ud::trust::checked, ud::trust::unchecked}) {
                auto const by_hand =
                    typename Table::view_type({source.values().data(), source.values().size()}, source.index(), t);
                iv_require_same(by_hand, source, n);
            }

            // the owning constructor from copies of both arrays, then it keeps working as a map
            auto blocks = iv_copy_index(source);
            auto values = source.values();
            auto loaded = Table(std::move(values), iv_as_view<Table>(blocks), ud::trust::checked);
            iv_require_same(loaded, source, n);
            REQUIRE(loaded.verify(ud::verify_level::full));

            // the owning constructor from a view, whatever trust it was built with
            auto const unchecked = typename Table::view_type(
                {source.values().data(), source.values().size()}, source.index(), ud::trust::unchecked);
            auto from_view = Table(unchecked);
            iv_require_same(from_view, source, n);

            for (std::size_t i = n; i < n + 200; ++i) {
                iv_insert(loaded, i);
            }
            for (std::size_t i = 0; i < n + 200; i += 2) {
                loaded.erase(iv_make_key<typename Table::key_type>(i));
            }
            REQUIRE(loaded.verify(ud::verify_level::full));
            for (std::size_t i = 0; i < n + 200; ++i) {
                auto const key = iv_make_key<typename Table::key_type>(i);
                auto const was_there = i >= n || !(with_erases && i % 3 == 0);
                REQUIRE(loaded.contains(key) == (i % 2 == 1 && was_there));
            }
        }
    }
}

TEST_CASE("index_view_empty_table") {
    auto const empty = ud::map<std::uint64_t, std::uint64_t>();
    REQUIRE(empty.index().data() != nullptr); // the sentinel, never null
    REQUIRE(empty.index().size() == 0);
    auto values = std::vector<std::pair<std::uint64_t, std::uint64_t>>();
    auto const loaded = ud::map<std::uint64_t, std::uint64_t>(std::move(values), empty.index(), ud::trust::checked);
    REQUIRE(loaded.empty());
    REQUIRE(!loaded.contains(1));
    auto const view = ud::map_view<std::uint64_t, std::uint64_t>({nullptr, 0}, {nullptr, 0}, ud::trust::checked);
    REQUIRE(view.empty());
    REQUIRE(view.find(1) == view.end());
    REQUIRE(view.verify(ud::verify_level::full));

    // default constructed, and what a move leaves behind
    auto const def = ud::map_view<std::uint64_t, std::uint64_t>();
    REQUIRE(def.empty());
    REQUIRE(def.begin() == def.end());
    REQUIRE(def.find(1) == def.end());
    auto const source = iv_make_table<ud::map<std::uint64_t, std::uint64_t>>(100, false);
    auto from = source.view();
    auto const to = std::move(from);
    REQUIRE(to.size() == 100);
    REQUIRE(from.empty()); // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
    REQUIRE(from.index().size() == 0);
    REQUIRE(from.find(iv_make_key<std::uint64_t>(1)) == from.end());
}

namespace {
template <typename T>
using detect_view = decltype(std::declval<T const&>().view());
} // namespace

// view() is for values in one array
static_assert(ud::detail::is_detected_v<detect_view, ud::map<int, int>>);
static_assert(!ud::detail::is_detected_v<detect_view, ud::segmented_map<int, int>>);
static_assert(!ud::detail::is_detected_v<detect_view, ud::map_view<int, int>>);

TEST_CASE("index_view_segmented_map_owning") {
    using seg = ud::segmented_map<std::uint64_t, std::size_t>;
    auto const source = iv_make_table<seg>(2000, true);
    auto blocks = iv_copy_index(source);
    auto values = source.values();
    auto loaded = seg(std::move(values), iv_as_view<seg>(blocks), ud::trust::checked);
    iv_require_same(loaded, source, 2000);
    loaded.try_emplace(iv_make_key<std::uint64_t>(5000), 1);
    REQUIRE(loaded.verify(ud::verify_level::full));

    // a flat view of the segmented values, by copying them out
    auto flat = std::vector<seg::value_type>(source.begin(), source.end());
    auto const v = ud::map_view<std::uint64_t, std::size_t>({flat.data(), flat.size()}, source.index(), ud::trust::checked);
    iv_require_same(v, source, 2000);
    auto const back = seg(v);
    iv_require_same(back, source, 2000);
}

TEST_CASE("index_view_written_to_a_file_and_read_back") {
    using map_t = ud::map<std::uint64_t, std::uint64_t>;
    auto const source = iv_make_table<map_t>(5000, true);
    auto const path = std::filesystem::temp_directory_path() / "udm_index_view_round_trip.bin";
    {
        auto f = std::ofstream(path, std::ios::binary);
        auto const id = map_t::index_format_id;
        auto const nv = source.size();
        auto const nb = source.index().size();
        f.write(reinterpret_cast<char const*>(&id), sizeof(id));
        f.write(reinterpret_cast<char const*>(&nv), sizeof(nv));
        f.write(reinterpret_cast<char const*>(&nb), sizeof(nb));
        f.write(reinterpret_cast<char const*>(source.values().data()),
                static_cast<std::streamsize>(nv * sizeof(map_t::value_type)));
        f.write(reinterpret_cast<char const*>(source.index().data()),
                static_cast<std::streamsize>(nb * sizeof(map_t::index_block)));
    }
    auto f = std::ifstream(path, std::ios::binary);
    auto id = std::uint64_t{};
    auto nv = std::size_t{};
    auto nb = std::size_t{};
    f.read(reinterpret_cast<char*>(&id), sizeof(id));
    f.read(reinterpret_cast<char*>(&nv), sizeof(nv));
    f.read(reinterpret_cast<char*>(&nb), sizeof(nb));
    REQUIRE(id == map_t::index_format_id);
    auto values = std::vector<map_t::value_type>(nv);
    auto blocks = std::vector<map_t::index_block>(nb);
    f.read(reinterpret_cast<char*>(values.data()), static_cast<std::streamsize>(nv * sizeof(map_t::value_type)));
    f.read(reinterpret_cast<char*>(blocks.data()), static_cast<std::streamsize>(nb * sizeof(map_t::index_block)));
    REQUIRE(f);
    f.close();
    std::filesystem::remove(path);

    auto const view = ud::map_view<std::uint64_t, std::uint64_t>(
        {values.data(), values.size()}, {blocks.data(), blocks.size()}, ud::trust::checked);
    iv_require_same(view, source, 5000);
    auto const owning = map_t(std::move(values), {blocks.data(), blocks.size()}, ud::trust::checked);
    iv_require_same(owning, source, 5000);
}

// Every rejection of the shape and of the slot check, owning and view.
TEST_CASE("index_view_rejects") {
    using map_t = ud::map<std::uint64_t, std::uint64_t>;
    using view_t = ud::map_view<std::uint64_t, std::uint64_t>;
    auto const source = iv_make_table<map_t>(1000, false);
    auto const good = iv_copy_index(source);
    auto const& vals = source.values();
    REQUIRE(good.size() >= 8);

    auto rejects = [&](map_t::index_view idx, std::vector<map_t::value_type> const& values) {
        auto copy = values;
        REQUIRE_THROWS_AS(map_t(std::move(copy), idx, ud::trust::checked), std::invalid_argument);
        REQUIRE(copy.size() == values.size()); // the caller's values are left alone
        REQUIRE_THROWS_AS(view_t({values.data(), values.size()}, idx, ud::trust::checked), std::invalid_argument);
    };

    // not a power of two
    rejects({good.data(), good.size() - 1}, vals);
    // the same two with an index that would pass every slot check: empty blocks and no values
    {
        auto const empty_blocks = std::vector<map_t::index_block>(8);
        auto none = std::vector<map_t::value_type>();
        REQUIRE_THROWS_AS(map_t(std::move(none), {empty_blocks.data(), 6}, ud::trust::checked), std::invalid_argument);
        REQUIRE_THROWS_AS(map_t(std::move(none), {empty_blocks.data(), 2}, ud::trust::checked), std::invalid_argument);
        REQUIRE_THROWS_AS(view_t({nullptr, 0}, {empty_blocks.data(), 2}, ud::trust::unchecked), std::invalid_argument);
        auto const ok = map_t(std::move(none), {empty_blocks.data(), 4}, ud::trust::checked);
        REQUIRE(ok.bucket_count() == 64);
        auto const ok8 = map_t(std::move(none), {empty_blocks.data(), 8}, ud::trust::checked);
        REQUIRE(ok8.bucket_count() == 128);
    }
    // a power of two, but below the smallest array
    rejects({good.data(), 2}, vals);
    // no index for values that are there
    rejects({nullptr, 0}, vals);
    // misaligned
    {
        auto bytes = std::vector<unsigned char>(good.size() * sizeof(map_t::index_block) + alignof(map_t::index_block));
        auto* misaligned = bytes.data() + 1;
        while (reinterpret_cast<std::uintptr_t>(misaligned) % alignof(map_t::index_block) == 0) {
            ++misaligned;
        }
        std::memcpy(misaligned, good.data(), good.size() * sizeof(map_t::index_block));
        rejects({reinterpret_cast<map_t::index_block const*>(misaligned), good.size()}, vals);
    }
    // a slot pointing past the values
    {
        auto bad = good;
        auto const at = iv_slot_of(bad, 17);
        bad[at.first].m_index[at.second] = static_cast<std::uint32_t>(vals.size());
        rejects({bad.data(), bad.size()}, vals);
        // and the owning table built from an unchecked view checks anyway
        auto const unchecked = view_t({vals.data(), vals.size()}, {bad.data(), bad.size()}, ud::trust::unchecked);
        REQUIRE_THROWS_AS(static_cast<void>(map_t(unchecked)), std::invalid_argument);
    }
    // fewer full slots than values
    {
        auto bad = good;
        auto const at = iv_slot_of(bad, 17);
        bad[at.first].m_fingerprints[at.second] = 0;
        rejects({bad.data(), bad.size()}, vals);
    }
    // more full slots than values
    {
        auto fewer = std::vector<map_t::value_type>(vals.begin(), vals.end() - 1);
        auto bad = good;
        auto const at = iv_slot_of(bad, vals.size() - 1);
        bad[at.first].m_index[at.second] = 0; // still in range of the shorter values
        rejects({bad.data(), bad.size()}, fewer);
    }
    // two slots pointing at one value, and so one value with none: an owning table rejects it,
    // because an erase that moves the doubly pointed value repoints only one slot and leaves the
    // other pointing past the end. A view only reads, and every slot is in range, so it accepts.
    {
        auto bad = good;
        auto const at = iv_slot_of(bad, 17);
        // 63: the last bit of a word of the bitmap
        bad[at.first].m_index[at.second] = 63;
        auto copy = vals;
        REQUIRE_THROWS_AS(map_t(std::move(copy), {bad.data(), bad.size()}, ud::trust::checked), std::invalid_argument);
        auto const v = view_t({vals.data(), vals.size()}, {bad.data(), bad.size()}, ud::trust::checked);
        REQUIRE(!v.verify(ud::verify_level::full));
        REQUIRE_THROWS_AS(static_cast<void>(map_t(v)), std::invalid_argument);
    }
    // trust::unchecked takes the bytes as they are: the shape is still checked, the slots are not
    {
        auto bad = good;
        auto const at = iv_slot_of(bad, 17);
        bad[at.first].m_fingerprints[at.second] = 0; // one value fewer than full slots
        auto copy = vals;
        auto const unchecked = map_t(std::move(copy), {bad.data(), bad.size()}, ud::trust::unchecked);
        REQUIRE(unchecked.size() == vals.size());
        REQUIRE(!unchecked.verify(ud::verify_level::full));
        auto copy2 = vals;
        REQUIRE_THROWS_AS(map_t(std::move(copy2), {good.data(), 2}, ud::trust::unchecked), std::invalid_argument);
        auto copy3 = vals;
        auto const fine = map_t(std::move(copy3), {good.data(), good.size()}, ud::trust::unchecked);
        REQUIRE(fine.verify(ud::verify_level::full));
    }
    // values the view cannot read as value_type
    {
        auto bytes = std::vector<unsigned char>((vals.size() + 1) * sizeof(map_t::value_type));
        auto* misaligned = bytes.data() + 1;
        REQUIRE_THROWS_AS(view_t({reinterpret_cast<map_t::value_type const*>(misaligned), 1},
                                 {good.data(), good.size()},
                                 ud::trust::unchecked),
                          std::invalid_argument);
    }
}

namespace {

enum class iv_enum : std::uint64_t {};

// A hash with a known format but not avalanching: mixed_hash() runs it through hash_int().
struct iv_plain_known_hash {
    static constexpr std::uint64_t format_id = 7;
    auto operator()(std::uint64_t k) const noexcept -> std::uint64_t {
        return k;
    }
};

// A hasher with state the format id cannot see.
struct iv_seeded_hash {
    using is_avalanching = void;
    std::uint64_t seed = 0;
    auto operator()(std::uint64_t k) const noexcept -> std::uint64_t {
        return ud::detail::hash_int(k ^ seed);
    }
};

} // namespace

TEST_CASE("index_view_verify") {
    using map_t = ud::map<std::uint64_t, std::uint64_t>;
    auto const source = iv_make_table<map_t>(1000, false);

    // a hasher constructed differently: the bytes pass the check and fail verify
    {
        using seeded = ud::map<std::uint64_t, std::uint64_t, iv_seeded_hash>;
        auto s = seeded(0, iv_seeded_hash{1});
        for (std::size_t i = 0; i < 1000; ++i) {
            s.try_emplace(iv_make_key<std::uint64_t>(i), i);
        }
        auto values = s.values();
        auto blocks = iv_copy_index(s);
        auto const wrong = seeded(std::move(values), iv_as_view<seeded>(blocks), ud::trust::checked, iv_seeded_hash{2});
        REQUIRE(!wrong.verify(ud::verify_level::spot));
        REQUIRE(!wrong.verify(ud::verify_level::full));
    }

    // spots for 1000 values are floor(j * 1000 / 16): 0, 62, 125, ..., 875, 937. Two slots swapped
    // at the last spot fail both levels; two swapped between spots fail only `full`.
    {
        auto bad = iv_copy_index(source);
        iv_swap_slots(bad, 937, 938);
        auto const v =
            map_t::view_type({source.values().data(), source.values().size()}, {bad.data(), bad.size()}, ud::trust::checked);
        REQUIRE(!v.verify(ud::verify_level::spot));
        REQUIRE(!v.verify(ud::verify_level::full));
    }
    {
        auto bad = iv_copy_index(source);
        iv_swap_slots(bad, 700, 701);
        auto const v =
            map_t::view_type({source.values().data(), source.values().size()}, {bad.data(), bad.size()}, ud::trust::checked);
        REQUIRE(v.verify(ud::verify_level::spot));
        REQUIRE(!v.verify(ud::verify_level::full));
    }
    // only value 0 inconsistent: it carries value 1's key
    {
        auto values = std::vector<map_t::value_type>(source.values().begin(), source.values().end());
        values[0].first = values[1].first;
        auto const v = map_t::view_type({values.data(), values.size()}, source.index(), ud::trust::checked);
        REQUIRE(!v.verify(ud::verify_level::spot));
        REQUIRE(!v.verify(ud::verify_level::full));
    }
    // duplicate keys: two values with one key, each slot pointing at its own
    {
        auto values = std::vector<map_t::value_type>(source.values().begin(), source.values().end());
        values[701].first = values[700].first;
        auto const v = map_t::view_type({values.data(), values.size()}, source.index(), ud::trust::checked);
        REQUIRE(!v.verify(ud::verify_level::full));
    }
}

// An index that passes the check but not verify(full), loaded into an owning table: the erase that
// has to repoint the last value cannot find its slot, and says so rather than corrupting the table.
TEST_CASE("index_view_erase_on_unverified_bytes_throws") {
    using map_t = ud::map<std::uint64_t, std::uint64_t>;
    auto const source = iv_make_table<map_t>(1000, false);
    auto bad = iv_copy_index(source);
    iv_swap_slots(bad, 999, 998);
    auto values = source.values();
    auto loaded = map_t(std::move(values), iv_as_view<map_t>(bad), ud::trust::checked);
    REQUIRE(!loaded.verify(ud::verify_level::full));
    REQUIRE_THROWS_AS(loaded.erase(iv_make_key<std::uint64_t>(0)), std::logic_error);
}

TEST_CASE("index_format_id") {
    using u64_group = ud::map<std::uint64_t, std::uint64_t>;
    using u64_big = ud::map<std::uint64_t,
                            std::uint64_t,
                            ud::hash<std::uint64_t>,
                            std::equal_to<std::uint64_t>,
                            std::allocator<std::pair<std::uint64_t, std::uint64_t>>,
                            ud::bucket_type::group_big>;
    // the values are not in it
    static_assert(u64_group::index_format_id == ud::map<std::uint64_t, std::string>::index_format_id);
    static_assert(u64_group::index_format_id == ud::set<std::uint64_t>::index_format_id);
    static_assert(u64_group::index_format_id == ud::map_view<std::uint64_t, std::uint64_t>::index_format_id);
    static_assert(ud::map<std::string, int>::index_format_id == ud::set<std::string_view>::index_format_id);
    // the index layout and the hash are
    // group_big's value index is a size_t, so on a 32 bit target the two blocks are the same bytes
    static_assert((sizeof(std::size_t) == 4) == (u64_group::index_format_id == u64_big::index_format_id));
    static_assert(u64_group::index_format_id != ud::map<std::string, std::uint64_t>::index_format_id);
    static_assert(ud::map<std::string, int>::index_format_id != ud::map<std::u16string, int>::index_format_id);
    static_assert(u64_group::index_format_id != ud::map<std::uint64_t, std::uint64_t, iv_seeded_hash>::index_format_id);
    // a pair of known hashes is known, a pair holding one that is not is not
    static_assert(ud::detail::hash_format_id<ud::hash<std::pair<int, std::string>>>() != 0);
    static_assert(ud::detail::hash_format_id<ud::hash<std::pair<int, std::string>>>() !=
                  ud::detail::hash_format_id<ud::hash<std::pair<std::string, int>>>());
    static_assert(ud::detail::hash_format_id<ud::hash<std::pair<int, iv_seeded_hash>>>() == 0);
    static_assert(ud::detail::hash_format_id<iv_seeded_hash>() == 0);
    // an enum hashes as its integer does
    static_assert(ud::map<iv_enum, int>::index_format_id == ud::map<std::uint64_t, int>::index_format_id);
    static_assert(ud::detail::hash_format_id<ud::hash<iv_enum>>() == ud::detail::integer_hash_format_id);
    static_assert(ud::detail::hash_format_id<ud::hash<int*>>() == 0);
}

// The golden index files in data/index_format/, written on x86-64: a set<uint64_t> and a
// set<std::string> of 2000 keys each, both bucket types, each with the index_format_id it was
// written under. Where the stored id equals today's, the bytes have to load and pass verify(full);
// that is what catches a change to how keys are placed or found that forgot to bump
// index_layout_version or hash_version. Where it differs, the format changed on purpose (or this
// is a 32 bit target reading a group_big file) and the test says to regenerate. On the -m32 legs
// the `group` files are the test that an index written on 64 bit reads on 32 bit.
//
// Regenerate: UDM_WRITE_INDEX_FORMAT=data/index_format ./udm-test -tc=index_format_golden_write
//
// File layout, native endian: u64 format id, u64 value count, u64 block count, the values (a
// uint64_t each, or a u32 length and the bytes for a string), the blocks.
namespace {

using iv_golden_u64 = ud::set<std::uint64_t>;
using iv_golden_u64_big = ud::set<std::uint64_t,
                                  ud::hash<std::uint64_t>,
                                  std::equal_to<std::uint64_t>,
                                  std::allocator<std::uint64_t>,
                                  ud::bucket_type::group_big>;
using iv_golden_str = ud::set<std::string>;
using iv_golden_str_big = ud::set<std::string,
                                  ud::hash<std::string>,
                                  std::equal_to<std::string>,
                                  std::allocator<std::string>,
                                  ud::bucket_type::group_big>;

constexpr std::size_t iv_golden_keys = 2000;

auto iv_golden_dir() -> std::filesystem::path {
    return fuzz::detail::corpus_base_dir() / ".." / "index_format";
}

template <typename Set>
void iv_write_golden(std::filesystem::path const& file) {
    auto s = iv_make_table<Set>(iv_golden_keys, false);
    auto f = std::ofstream(file, std::ios::binary);
    auto put = [&](auto x) {
        f.write(reinterpret_cast<char const*>(&x), sizeof(x));
    };
    put(std::uint64_t{Set::index_format_id});
    put(std::uint64_t{s.size()});
    put(std::uint64_t{s.index().size()});
    for (auto const& k : s.values()) {
        if constexpr (std::is_same_v<typename Set::key_type, std::string>) {
            put(static_cast<std::uint32_t>(k.size()));
            f.write(k.data(), static_cast<std::streamsize>(k.size()));
        } else {
            put(k);
        }
    }
    f.write(reinterpret_cast<char const*>(s.index().data()),
            static_cast<std::streamsize>(s.index().size() * sizeof(typename Set::index_block)));
}

template <typename Set>
void iv_check_golden(char const* name) {
    INFO("file " << std::string(name));
    auto f = std::ifstream(iv_golden_dir() / name, std::ios::binary);
    REQUIRE(f);
    auto get = [&](auto& x) {
        f.read(reinterpret_cast<char*>(&x), sizeof(x));
    };
    auto id = std::uint64_t{};
    auto nv = std::uint64_t{};
    auto nb = std::uint64_t{};
    get(id);
    get(nv);
    get(nb);
    if (id != Set::index_format_id) {
        MESSAGE(
            std::string(name) << " was written under another index_format_id; if the format changed on purpose, regenerate it "
                                 "(see the comment above this test)");
        return;
    }
    auto values = std::vector<typename Set::key_type>();
    for (std::uint64_t i = 0; i < nv; ++i) {
        if constexpr (std::is_same_v<typename Set::key_type, std::string>) {
            auto len = std::uint32_t{};
            get(len);
            auto k = std::string(len, '\0');
            f.read(k.data(), static_cast<std::streamsize>(len));
            values.push_back(std::move(k));
        } else {
            auto k = std::uint64_t{};
            get(k);
            values.push_back(k);
        }
    }
    auto blocks = std::vector<typename Set::index_block>(static_cast<std::size_t>(nb));
    f.read(reinterpret_cast<char*>(blocks.data()), static_cast<std::streamsize>(nb * sizeof(typename Set::index_block)));
    REQUIRE(f);

    if constexpr (!std::is_same_v<typename Set::key_type, std::string>) {
        auto const view =
            typename Set::view_type({values.data(), values.size()}, {blocks.data(), blocks.size()}, ud::trust::checked);
        REQUIRE(view.verify(ud::verify_level::full));
    }
    auto const loaded = Set(std::move(values), {blocks.data(), blocks.size()}, ud::trust::checked);
    REQUIRE(loaded.size() == iv_golden_keys);
    REQUIRE(loaded.verify(ud::verify_level::full));
    for (std::size_t i = 0; i < iv_golden_keys; ++i) {
        REQUIRE(loaded.contains(iv_make_key<typename Set::key_type>(i)));
    }
}

constexpr bool iv_little_endian = ud::detail::native_endian_id == 1;

template <typename T>
struct iv_type {
    using type = T;
};

// The four golden files and the set each one holds, one list for reading and writing.
template <typename F>
void iv_for_each_golden(F f) {
    f(iv_type<iv_golden_u64>{}, "set_u64_group.bin");
    f(iv_type<iv_golden_u64_big>{}, "set_u64_group_big.bin");
    f(iv_type<iv_golden_str>{}, "set_str_group.bin");
    f(iv_type<iv_golden_str_big>{}, "set_str_group_big.bin");
}

} // namespace

TEST_CASE("index_format_golden") {
    if constexpr (!iv_little_endian) {
        MESSAGE("skipped: the golden index files are little endian");
        return;
    }
    iv_for_each_golden([](auto tag, char const* name) {
        iv_check_golden<typename decltype(tag)::type>(name);
    });
}

TEST_CASE("index_format_golden_write") {
    auto const dir = fuzz::detail::env("UDM_WRITE_INDEX_FORMAT");
    if (!dir) {
        return;
    }
    auto const d = std::filesystem::path(*dir);
    std::filesystem::create_directories(d);
    iv_for_each_golden([&](auto tag, char const* name) {
        iv_write_golden<typename decltype(tag)::type>(d / name);
    });
}

// The ids pinned, so that a change to anything folded into them is a decision: it makes every saved
// index unreadable. Little endian only; the `group` ids are the same on 32 and 64 bit.
TEST_CASE("index_format_id_pinned") {
    if constexpr (!iv_little_endian) {
        MESSAGE("skipped: the pinned ids are little endian");
        return;
    }
    REQUIRE(ud::map<std::uint64_t, int>::index_format_id == UINT64_C(0x282025bf1d34899));
    REQUIRE(ud::map<std::string, int>::index_format_id == UINT64_C(0xc00140633c96de64));
    REQUIRE(ud::set<std::pair<int, std::string>>::index_format_id == UINT64_C(0x2eede0b40bba2889));
    REQUIRE(ud::map<std::uint64_t, int, iv_plain_known_hash>::index_format_id == UINT64_C(0xd79204202f40229a));
    using big = ud::map<std::uint64_t,
                        int,
                        ud::hash<std::uint64_t>,
                        std::equal_to<std::uint64_t>,
                        std::allocator<std::pair<std::uint64_t, int>>,
                        ud::bucket_type::group_big>;
    REQUIRE(big::index_format_id == (sizeof(std::size_t) == 8 ? UINT64_C(0xc66c70dc1ff9d17e) : UINT64_C(0x282025bf1d34899)));
}

// Either allocation of the owning load can fail: the bitmap first, then the block array. The table
// is never built, nothing leaks (LeakSanitizer in the sanitizer build), and the caller keeps the
// values.
TEST_CASE("index_view_owning_load_allocation_failure") {
#if ANKERL_TEST_CAN_BOMB_ALLOCATIONS()
    using pair_t = std::pair<std::uint64_t, std::uint64_t>;
    using map_t = ud::map<std::uint64_t,
                          std::uint64_t,
                          ud::hash<std::uint64_t>,
                          std::equal_to<std::uint64_t>,
                          test::bombing_allocator<pair_t>>;
    auto const source = iv_make_table<map_t>(500, false);
    auto blocks = iv_copy_index(source);
    for (int bomb = 0; bomb < 2; ++bomb) {
        CAPTURE(bomb);
        auto values = source.values();
        {
            auto const b = test::bomb_after(bomb);
            REQUIRE_THROWS_AS(map_t(std::move(values), iv_as_view<map_t>(blocks), ud::trust::checked), std::bad_alloc);
        }
        REQUIRE(values.size() == 500);
    }
    auto values = source.values();
    auto const loaded = map_t(std::move(values), iv_as_view<map_t>(blocks), ud::trust::checked);
    REQUIRE(loaded.verify(ud::verify_level::full));
#endif
}
