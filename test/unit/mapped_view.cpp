#include <ankerl/mapped_view.h>
#include <ankerl/unordered_dense.h>

#include <app/doctest.h>
#include <fuzz/run.h> // for fuzz::detail::env

#if ANKERL_UNORDERED_DENSE_HAS_MAPPED_VIEW

#    include <cstddef>
#    include <cstdint>
#    include <cstring>
#    include <filesystem>
#    include <fstream>
#    include <limits>
#    include <stdexcept>
#    include <string>
#    include <system_error>
#    include <type_traits>
#    include <utility>
#    include <vector>

#    include <fcntl.h>    // for open
#    include <sys/mman.h> // for mmap, to write a file on hugetlbfs, which has no write()
#    include <unistd.h>   // for getpid, ftruncate, close

// A file holding a map's two arrays, mapped back three ways, read through the view and checked
// against the map it was written from (#301).
namespace {

namespace ud = ankerl::unordered_dense;

using mv_map_t = ud::map<std::uint64_t, std::uint64_t>;
using mv_view_t = ud::map_view<std::uint64_t, std::uint64_t>;
using mv_set_view_t = ud::set_view<std::uint64_t>;

constexpr auto mv_all = {ud::mapping::file, ud::mapping::file_populated, ud::mapping::huge_copy};

// view() on a temporary does not compile: the reference would outlive the mapping.
template <typename T, typename = void>
struct mv_view_on_rvalue : std::false_type {};
template <typename T>
struct mv_view_on_rvalue<T, std::void_t<decltype(std::declval<T&&>().view())>> : std::true_type {};
static_assert(!mv_view_on_rvalue<ud::mapped_view<mv_view_t>>::value);
static_assert(std::is_same_v<decltype(std::declval<ud::mapped_view<mv_view_t> const&>().view()), mv_view_t const&>);
static_assert(!std::is_copy_constructible_v<ud::mapped_view<mv_view_t>>);
static_assert(std::is_nothrow_move_constructible_v<ud::mapped_view<mv_view_t>>);
static_assert(!std::is_move_assignable_v<ud::mapped_view<mv_view_t>>);
static_assert(std::is_nothrow_move_assignable_v<ud::mapped_file>);
static_assert(!std::is_copy_constructible_v<ud::mapped_file>);

auto mv_path(char const* name) -> std::string {
    return (std::filesystem::temp_directory_path() /
            (std::string("udm_mapped_view_") + name + "_" + std::to_string(getpid()) + ".bin"))
        .string();
}

// The values at 4096, the index at the next 4096 after them: the layout the caller would choose.
template <typename Table>
auto mv_write(std::string const& path, Table const& m) -> ud::mapped_layout {
    auto const& values = m.values();
    auto const index = m.index();
    auto layout = ud::mapped_layout{};
    layout.values_offset = 4096;
    layout.num_values = values.size();
    layout.index_offset = (layout.values_offset + (values.size() * sizeof(values[0])) + 4095) / 4096 * 4096;
    layout.num_blocks = index.size();
    auto bytes = std::vector<char>(layout.index_offset + (index.size() * sizeof(typename Table::index_block)));
    // an empty map's values() has a null data(), which memcpy must not get
    if (!values.empty()) {
        std::memcpy(bytes.data() + layout.values_offset, values.data(), values.size() * sizeof(values[0]));
    }
    if (!index.empty()) {
        std::memcpy(bytes.data() + layout.index_offset,
                    static_cast<void const*>(index.data()),
                    index.size() * sizeof(typename Table::index_block));
    }
    auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return layout;
}

auto mv_make(std::uint64_t n, std::uint64_t salt) -> mv_map_t {
    auto m = mv_map_t();
    for (std::uint64_t i = 0; i < n; ++i) {
        m.try_emplace((i * UINT64_C(0x9E3779B97F4A7C15)) ^ salt, i + salt);
    }
    return m;
}

// Whether this process still has a mapping of the file.
auto mv_is_mapped(std::string const& path) -> bool {
    auto in = std::ifstream("/proc/self/maps");
    auto line = std::string();
    while (std::getline(in, line)) {
        if (line.find(path) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// Removes the file on every way out of a test, a failed REQUIRE included: a file left on hugetlbfs
// holds its reserved pages.
struct mv_remove_on_exit {
    std::string path;
    mv_remove_on_exit(mv_remove_on_exit const&) = delete;
    mv_remove_on_exit(mv_remove_on_exit&&) = delete;
    auto operator=(mv_remove_on_exit const&) -> mv_remove_on_exit& = delete;
    auto operator=(mv_remove_on_exit&&) -> mv_remove_on_exit& = delete;
    ~mv_remove_on_exit() {
        auto ec = std::error_code();
        std::filesystem::remove(path, ec);
    }
};

void mv_require_same(mv_view_t const& v, mv_map_t const& m) {
    REQUIRE(v.size() == m.size());
    auto wrong = std::size_t{0};
    for (auto const& [k, val] : m) {
        auto it = v.find(k);
        wrong += (it == v.end() || it->second != val) ? 1U : 0U;
    }
    REQUIRE(wrong == 0);
    REQUIRE(v.verify(ud::verify_level::full));
}

} // namespace

TEST_CASE("mapped_view_round_trip") {
    auto const path = mv_path("round_trip");
    // 150000 entries is a 4 MB file: huge_copy takes its 2 MB path there and its plain one at 10000
    for (std::uint64_t n : {std::uint64_t{10000}, std::uint64_t{150000}}) {
        auto const m = mv_make(n, 0);
        auto const layout = mv_write(path, m);
        REQUIRE((std::filesystem::file_size(path) >= (std::size_t{2} << 20U)) == (n == 150000));
        for (auto how : mv_all) {
            for (auto t : {ud::trust::checked, ud::trust::unchecked}) {
                {
                    auto const mv = ud::mapped_view<mv_view_t>(path.c_str(), layout, t, how);
                    mv_require_same(mv.view(), m);
                    REQUIRE(mv.view().find(UINT64_C(0xDEAD)) == mv.view().end());
                    REQUIRE(mv.file().size() == std::filesystem::file_size(path));
                    REQUIRE(mv_is_mapped(path) == (how != ud::mapping::huge_copy));
                }
                REQUIRE(!mv_is_mapped(path)); // the destructor unmapped all of it
            }
        }
    }
    std::filesystem::remove(path);
}

TEST_CASE("mapped_view_set") {
    auto const path = mv_path("set");
    auto s = ud::set<std::uint64_t>();
    for (std::uint64_t i = 0; i < 3000; ++i) {
        s.insert(i * 7);
    }
    auto const layout = mv_write(path, s);
    auto const mv =
        ud::mapped_view<mv_set_view_t>(ud::mapped_file(path.c_str(), ud::mapping::huge_copy), layout, ud::trust::checked);
    REQUIRE(mv.view().size() == s.size());
    auto wrong = std::size_t{0};
    for (std::uint64_t i = 0; i < 3000 * 7; ++i) {
        wrong += mv.view().contains(i) != (i % 7 == 0) ? 1U : 0U;
    }
    REQUIRE(wrong == 0);
    std::filesystem::remove(path);
}

// What the header tells writers to do: a new file renamed into place. A mapping keeps the old
// file's bytes, the next mapping gets the new ones.
TEST_CASE("mapped_view_rename_into_place") {
    auto const path = mv_path("rename");
    auto const tmp = path + ".new";
    auto const old_map = mv_make(5000, 0);
    auto const new_map = mv_make(7000, 12345);
    auto const old_layout = mv_write(path, old_map);
    for (auto how : mv_all) {
        auto const before = ud::mapped_view<mv_view_t>(path.c_str(), old_layout, ud::trust::checked, how);
        auto const new_layout = mv_write(tmp, new_map);
        std::filesystem::rename(tmp, path);
        auto const after = ud::mapped_view<mv_view_t>(path.c_str(), new_layout, ud::trust::checked, how);
        mv_require_same(before.view(), old_map);
        mv_require_same(after.view(), new_map);
        mv_write(path, old_map);
    }
    std::filesystem::remove(path);
}

TEST_CASE("mapped_view_move") {
    auto const path = mv_path("move");
    auto const m = mv_make(2000, 3);
    auto const layout = mv_write(path, m);
    for (auto how : mv_all) {
        auto a = ud::mapped_view<mv_view_t>(path.c_str(), layout, ud::trust::checked, how);
        auto const* data = a.file().data();
        auto b = std::move(a);
        REQUIRE(b.file().data() == data); // the mapping did not move, so the view did not dangle
        mv_require_same(b.view(), m);
        REQUIRE(a.file().data() == nullptr); // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        REQUIRE(a.view().empty());           // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    }
    std::filesystem::remove(path);
}

TEST_CASE("mapped_view_empty") {
    auto const path = mv_path("empty");
    {
        auto out = std::ofstream(path, std::ios::binary | std::ios::trunc);
    }
    for (auto how : mv_all) {
        auto const mv = ud::mapped_view<mv_view_t>(path.c_str(), ud::mapped_layout{0, 0, 0, 0}, ud::trust::checked, how);
        REQUIRE(mv.file().data() == nullptr);
        REQUIRE(mv.file().size() == 0);
        REQUIRE(mv.view().empty());
        REQUIRE(mv.view().find(1) == mv.view().end());
    }
    // an empty map written as a file: values and index both empty, past a header
    auto const layout = mv_write(path, mv_map_t());
    auto const mv = ud::mapped_view<mv_view_t>(path.c_str(), layout, ud::trust::checked);
    REQUIRE(mv.view().empty());
    std::filesystem::remove(path);
}

TEST_CASE("mapped_view_rejects") {
    auto const path = mv_path("rejects");
    auto const m = mv_make(1000, 0);
    auto const layout = mv_write(path, m);
    auto const size = std::filesystem::file_size(path);

    REQUIRE_THROWS_AS(ud::mapped_file(mv_path("does_not_exist").c_str()), std::system_error);

    for (auto how : mv_all) {
        auto past = layout;
        past.num_blocks += 1; // one block past the end of the file
        REQUIRE_THROWS_AS(ud::mapped_view<mv_view_t>(path.c_str(), past, ud::trust::unchecked, how), std::invalid_argument);
        past = layout;
        // an element count whose byte count wraps around to 16
        past.num_values = (std::numeric_limits<std::size_t>::max() / sizeof(mv_map_t::value_type)) + 2;
        REQUIRE_THROWS_AS(ud::mapped_view<mv_view_t>(path.c_str(), past, ud::trust::unchecked, how), std::invalid_argument);
        past = layout;
        past.index_offset = static_cast<std::size_t>(size) + 4096; // an offset past the end
        REQUIRE_THROWS_AS(ud::mapped_view<mv_view_t>(path.c_str(), past, ud::trust::unchecked, how), std::invalid_argument);

        auto misaligned = layout;
        misaligned.values_offset += 1;
        REQUIRE_THROWS_AS(ud::mapped_view<mv_view_t>(path.c_str(), misaligned, ud::trust::unchecked, how),
                          std::invalid_argument);
        misaligned = layout;
        misaligned.index_offset -= 2; // the index is last in the file: moving it forward would reach past the end
        REQUIRE_THROWS_AS(ud::mapped_view<mv_view_t>(path.c_str(), misaligned, ud::trust::unchecked, how),
                          std::invalid_argument);

        // the values one short: the checked view finds a slot pointing past them
        auto short_values = layout;
        short_values.num_values -= 1;
        REQUIRE_THROWS_AS(ud::mapped_view<mv_view_t>(path.c_str(), short_values, ud::trust::checked, how),
                          std::invalid_argument);
    }
    std::filesystem::remove(path);
}

namespace {

// A field of /proc/meminfo in kB, or -1.
auto mv_meminfo_kb(char const* field) -> long {
    auto in = std::ifstream("/proc/meminfo");
    auto name = std::string();
    auto kb = long{0};
    while (in >> name >> kb) {
        if (name == field) {
            return kb;
        }
        in.ignore(64, '\n');
    }
    return -1;
}

} // namespace

// Needs a hugetlbfs mount with free reserved pages, named by UDM_HUGETLBFS_DIR; skipped otherwise.
// It counts HugePages_Free, so two processes running it at once fail each other:
//
//     sudo sysctl vm.nr_hugepages=64
//     sudo mount -t hugetlbfs -o uid=$(id -u),gid=$(id -g),pagesize=2M none /mnt/huge
//     UDM_HUGETLBFS_DIR=/mnt/huge ./udm-test -tc=mapped_view_hugetlbfs
TEST_CASE("mapped_view_hugetlbfs") {
    auto const dir = fuzz::detail::env("UDM_HUGETLBFS_DIR");
    if (!dir) {
        MESSAGE("UDM_HUGETLBFS_DIR is not set: skipped");
        return;
    }
    auto const m = mv_make(150000, 0); // a 4 MB file: two huge pages
    auto const src = mv_path("hugetlbfs_src");
    auto const remove_src = mv_remove_on_exit{src};
    auto const layout = mv_write(src, m);
    auto const size = static_cast<std::size_t>(std::filesystem::file_size(src));
    auto bytes = std::vector<char>(size);
    std::ifstream(src, std::ios::binary).read(bytes.data(), static_cast<std::streamsize>(size));

    // hugetlbfs takes only whole huge pages for a file's size, and data only through a mapping
    auto const path = *dir + "/udm_mapped_view_" + std::to_string(getpid()) + ".bin";
    auto const remove_path = mv_remove_on_exit{path};
    auto const file_size = (size + (std::size_t{2} << 20U) - 1) / (std::size_t{2} << 20U) * (std::size_t{2} << 20U);
    auto const fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644); // NOLINT(cppcoreguidelines-pro-type-vararg)
    REQUIRE(fd >= 0);
    REQUIRE(::ftruncate(fd, static_cast<off_t>(file_size)) == 0);
    auto* p = ::mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    REQUIRE(p != MAP_FAILED); // NOLINT(cppcoreguidelines-pro-type-cstyle-cast,performance-no-int-to-ptr)
    std::memcpy(p, bytes.data(), size);
    ::munmap(p, file_size);
    ::close(fd);

    {
        auto const mv = ud::mapped_view<mv_view_t>(path.c_str(), layout, ud::trust::checked);
        REQUIRE(mv.file().size() == file_size);
        mv_require_same(mv.view(), m);
        REQUIRE(mv_is_mapped(path));
    }
    REQUIRE(!mv_is_mapped(path)); // munmap takes only whole huge pages; it took this length

    // huge_copy of an ordinary file goes to MAP_HUGETLB while reserved pages are free
    auto const free_before = mv_meminfo_kb("HugePages_Free:");
    {
        auto const mv = ud::mapped_view<mv_view_t>(src.c_str(), layout, ud::trust::checked, ud::mapping::huge_copy);
        REQUIRE(mv_meminfo_kb("HugePages_Free:") == free_before - 2);
        mv_require_same(mv.view(), m);
    }
    REQUIRE(mv_meminfo_kb("HugePages_Free:") == free_before);
}

#endif
