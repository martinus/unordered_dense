#include <ankerl/unordered_dense.h>

#include <app/doctest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// A custom index container, named by the value container's `index_container<Bucket>` (#303).
//
// Two shapes. A read only one with self-relative members, drecouse's shape: the table object lives
// inside a blob next to its two arrays, and the blob can be copied anywhere and still read. And an
// owning one that forwards to std::vector, which has to insert and erase like a map.

namespace {

namespace ud = ankerl::unordered_dense;

auto ic_address(void const* p) -> std::uintptr_t {
    return reinterpret_cast<std::uintptr_t>(p); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

// A pointer stored as the distance from the object that holds it, so that the holder can be copied
// byte for byte together with what it points at. Copying it as an object recomputes the distance.
template <typename T>
class ic_offset_ptr {
    std::ptrdiff_t m_offset = 0; // 0 is null: an object never points at itself here
    std::size_t m_size = 0;

public:
    ic_offset_ptr() = default;
    ic_offset_ptr(T const* p, std::size_t n) {
        set(p, n);
    }
    ic_offset_ptr(ic_offset_ptr const& other) {
        set(other.get(), other.m_size);
    }
    auto operator=(ic_offset_ptr const& other) -> ic_offset_ptr& {
        set(other.get(), other.m_size);
        return *this;
    }
    // a move is a copy: the distance has to be recomputed either way
    ic_offset_ptr(ic_offset_ptr&& other) noexcept {
        set(other.get(), other.m_size);
    }
    auto operator=(ic_offset_ptr&& other) noexcept -> ic_offset_ptr& {
        set(other.get(), other.m_size);
        return *this;
    }
    ~ic_offset_ptr() = default;

    void set(T const* p, std::size_t n) {
        m_offset = p == nullptr ? 0 : static_cast<std::ptrdiff_t>(ic_address(p) - ic_address(this));
        m_size = n;
    }
    [[nodiscard]] auto get() const -> T const* {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
        return m_offset == 0 ? nullptr : reinterpret_cast<T const*>(ic_address(this) + static_cast<std::uintptr_t>(m_offset));
    }
    [[nodiscard]] auto size() const -> std::size_t {
        return m_size;
    }
};

// The read only index container: the contract's read only half.
template <typename Bucket>
class ic_offset_index {
public:
    using block = ud::detail::group_block<Bucket>;
    using allocator_type = std::allocator<block>;
    static constexpr bool nothrow_move_assignable = true;

private:
    ic_offset_ptr<block> m_blocks;

public:
    ic_offset_index() = default;
    template <typename A, typename = std::enable_if_t<!std::is_same_v<A, ic_offset_index>>>
    explicit ic_offset_index(A const& /*alloc*/) {}
    template <typename A>
    ic_offset_index(ic_offset_index const& other, A const& /*alloc*/)
        : m_blocks(other.m_blocks) {}

    [[nodiscard]] auto data() const -> block const* {
        return m_blocks.size() == 0 ? ud::detail::sentinel_blocks<Bucket>() : m_blocks.get();
    }
    [[nodiscard]] auto size() const -> std::size_t {
        return m_blocks.size();
    }
    [[nodiscard]] auto empty() const -> bool {
        return m_blocks.size() == 0;
    }
    void assign(ic_offset_index const& other) {
        m_blocks = other.m_blocks;
    }
    void assign(block const* data, std::size_t size) {
        m_blocks.set(data, size);
    }
    void clear() {
        m_blocks.set(nullptr, 0);
    }
};

// The read only value container with the same self-relative pointer, naming the index container.
template <typename T>
class ic_offset_values {
    ic_offset_ptr<T> m_values;

public:
    using is_view = void;
    using value_type = T;
    using iterator = T const*;
    using const_iterator = T const*;
    using reference = T const&;
    using const_reference = T const&;
    using pointer = T const*;
    using const_pointer = T const*;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using allocator_type = std::allocator<T>;
    template <typename Bucket>
    using index_container = ic_offset_index<Bucket>;

    ic_offset_values() = default;
    ic_offset_values(T const* data, std::size_t size)
        : m_values(data, size) {}
    explicit ic_offset_values(allocator_type const& /*alloc*/) {}
    ic_offset_values(ic_offset_values const& other, allocator_type const& /*alloc*/)
        : m_values(other.m_values) {}

    [[nodiscard]] auto get_allocator() const -> allocator_type {
        return {};
    }
    [[nodiscard]] auto data() const -> T const* {
        return m_values.get();
    }
    [[nodiscard]] auto size() const -> std::size_t {
        return m_values.size();
    }
    [[nodiscard]] auto empty() const -> bool {
        return m_values.size() == 0;
    }
    [[nodiscard]] auto begin() const -> T const* {
        return m_values.get();
    }
    [[nodiscard]] auto end() const -> T const* {
        return m_values.get() + m_values.size();
    }
    [[nodiscard]] auto operator[](std::size_t i) const -> T const& {
        return m_values.get()[i];
    }
    void clear() {
        m_values.set(nullptr, 0);
    }
};

// The owning index container: group_storage's members, forwarding to a std::vector.
template <typename Bucket>
class ic_vector_index {
public:
    using block = ud::detail::group_block<Bucket>;
    using allocator_type = std::allocator<block>;
    static constexpr bool nothrow_move_assignable = true;

private:
    std::vector<block> m_blocks;

public:
    template <typename A, typename = std::enable_if_t<!std::is_same_v<A, ic_vector_index>>>
    explicit ic_vector_index(A const& /*alloc*/) {}
    template <typename A>
    ic_vector_index(ic_vector_index&& other, A const& /*alloc*/) noexcept
        : m_blocks(std::move(other.m_blocks)) {}
    ic_vector_index(ic_vector_index const&) = delete;
    ic_vector_index(ic_vector_index&&) = delete;
    auto operator=(ic_vector_index const&) -> ic_vector_index& = delete;
    auto operator=(ic_vector_index&&) -> ic_vector_index& = delete;
    ~ic_vector_index() = default;

    void take(ic_vector_index& other) noexcept {
        m_blocks = std::move(other.m_blocks);
        other.m_blocks.clear();
    }
    void set_allocator(allocator_type const& /*alloc*/) noexcept {
        clear();
    }
    [[nodiscard]] auto get_allocator() const -> allocator_type {
        return {};
    }
    [[nodiscard]] auto empty() const -> bool {
        return m_blocks.empty();
    }
    [[nodiscard]] auto size() const -> std::size_t {
        return m_blocks.size();
    }
    void clear() noexcept {
        m_blocks = std::vector<block>();
    }
    void swap(ic_vector_index& other) noexcept {
        m_blocks.swap(other.m_blocks);
    }
    void resize(std::size_t num_groups) {
        m_blocks.assign(num_groups, block{});
    }
    void assign(ic_vector_index const& other) {
        m_blocks = other.m_blocks;
    }
    [[nodiscard]] auto data() -> block* {
        return m_blocks.empty() ? ud::detail::sentinel_blocks<Bucket>() : m_blocks.data();
    }
    [[nodiscard]] auto data() const -> block const* {
        return m_blocks.empty() ? ud::detail::sentinel_blocks<Bucket>() : m_blocks.data();
    }
    void clear_metadata() {
        for (auto& b : m_blocks) {
            static_cast<Bucket&>(b) = Bucket{};
        }
    }
};

template <typename T>
class ic_vector_values : public std::vector<T> {
public:
    using std::vector<T>::vector;
    template <typename Bucket>
    using index_container = ic_vector_index<Bucket>;
};

using ic_pair = std::pair<std::uint64_t, std::uint64_t>;

template <typename Bucket>
using ic_blob_table = ud::detail::table<std::uint64_t,
                                        std::uint64_t,
                                        ud::hash<std::uint64_t>,
                                        std::equal_to<std::uint64_t>,
                                        ic_offset_values<ic_pair>,
                                        Bucket,
                                        false>;

template <typename Bucket>
using ic_owning_table = ud::detail::table<std::uint64_t,
                                          std::uint64_t,
                                          ud::hash<std::uint64_t>,
                                          std::equal_to<std::uint64_t>,
                                          ic_vector_values<ic_pair>,
                                          Bucket,
                                          false>;

template <typename Bucket>
using ic_source_map = ud::
    map<std::uint64_t, std::uint64_t, ud::hash<std::uint64_t>, std::equal_to<std::uint64_t>, std::allocator<ic_pair>, Bucket>;

auto ic_key(std::size_t i) -> std::uint64_t {
    return static_cast<std::uint64_t>(i) * UINT64_C(0x9E3779B97F4A7C15);
}

template <typename A, typename B>
void ic_require_same(A const& a, B const& b, std::size_t n) {
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < n + 100; ++i) {
        auto const ia = a.find(ic_key(i));
        auto const ib = b.find(ic_key(i));
        REQUIRE((ia == a.end()) == (ib == b.end()));
        if (ia != a.end()) {
            REQUIRE(ia->second == ib->second);
        }
    }
}

constexpr auto ic_round_up(std::size_t x) -> std::size_t {
    return (x + 63U) / 64U * 64U;
}

} // namespace

// The policy picks the container the value container names, and leaves every other table alone.
static_assert(std::is_same_v<
              ud::detail::index_container_for<ud::bucket_type::group, ic_offset_values<ic_pair>, ic_offset_values<ic_pair>>,
              ic_offset_index<ud::bucket_type::group>>);
static_assert(
    std::is_same_v<
        ud::detail::index_container_for<ud::bucket_type::group_big, ic_vector_values<ic_pair>, ic_vector_values<ic_pair>>,
        ic_vector_index<ud::bucket_type::group_big>>);
static_assert(
    std::is_same_v<ud::detail::index_container_for<ud::bucket_type::group, std::allocator<ic_pair>, std::vector<ic_pair>>,
                   ud::detail::group_storage<ud::bucket_type::group, std::allocator<ic_pair>>>);
static_assert(std::is_same_v<ud::detail::index_container_for<ud::bucket_type::group,
                                                             ud::detail::view_container<ic_pair>,
                                                             ud::detail::view_container<ic_pair>>,
                             ud::detail::group_view<ud::bucket_type::group>>);
static_assert(
    std::is_same_v<
        ud::detail::index_container_for<ud::bucket_type::group, ud::segmented_vector<ic_pair>, ud::segmented_vector<ic_pair>>,
        ud::detail::group_storage<ud::bucket_type::group, std::allocator<ic_pair>>>);

TEST_CASE_TEMPLATE("index_container_table_inside_a_blob", Bucket, ud::bucket_type::group, ud::bucket_type::group_big) {
    using table_t = ic_blob_table<Bucket>;
    using block = ud::detail::group_block<Bucket>;
    auto source = ic_source_map<Bucket>();
    constexpr std::size_t n = 3000;
    for (std::size_t i = 0; i < n; ++i) {
        source.try_emplace(ic_key(i), i);
    }
    for (std::size_t i = 0; i < n; i += 7) {
        source.erase(ic_key(i));
    }
    auto const& values = source.values();
    auto const index = source.index();

    // [table object][values][index], each at a multiple of 64 from the start of the blob
    auto const values_at = ic_round_up(sizeof(table_t));
    auto const index_at = values_at + ic_round_up(values.size() * sizeof(ic_pair));
    auto const blob_size = index_at + (index.size() * sizeof(block));
    auto make_blob = [&] {
        return std::unique_ptr<unsigned char, void (*)(void*)>(
            static_cast<unsigned char*>(::operator new(blob_size, std::align_val_t{64})), [](void* p) {
                ::operator delete(p, std::align_val_t{64});
            });
    };
    auto blob = make_blob();
    std::memcpy(blob.get() + values_at, values.data(), values.size() * sizeof(ic_pair));
    std::memcpy(blob.get() + index_at, static_cast<void const*>(index.data()), index.size() * sizeof(block));

    auto* t = ::new (static_cast<void*>(blob.get()))
        table_t(ic_offset_values<ic_pair>(reinterpret_cast<ic_pair const*>(blob.get() + values_at), values.size()),
                typename table_t::index_view(reinterpret_cast<block const*>(blob.get() + index_at), index.size()),
                ud::trust::checked);
    ic_require_same(*t, source, n);
    REQUIRE(t->verify(ud::verify_level::full));
    // ic_offset_values has no cbegin(), and the table's must not need one
    REQUIRE(std::equal(t->cbegin(), t->cend(), source.begin(), source.end()));

    // The whole blob, table object included, copied byte for byte somewhere else: everything in it
    // is relative, so it reads the same there. This is the use the container exists for; the copy is
    // of an object that is not trivially copyable, which is the caller's contract with its own types.
    auto moved = make_blob();
    std::memcpy(moved.get(), blob.get(), blob_size);
    std::memset(blob.get(), 0, blob_size); // the original is gone
    auto const* t2 = std::launder(reinterpret_cast<table_t const*>(moved.get()));
    ic_require_same(*t2, source, n);
    REQUIRE(t2->verify(ud::verify_level::full));
    static_assert(std::is_trivially_destructible_v<table_t>);
}

TEST_CASE_TEMPLATE("index_container_owning_forwards_to_vector", Bucket, ud::bucket_type::group, ud::bucket_type::group_big) {
    using table_t = ic_owning_table<Bucket>;
    auto t = table_t();
    auto ref = std::unordered_map<std::uint64_t, std::uint64_t>();
    auto rng = std::uint64_t{1};
    auto next = [&] {
        rng = rng * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
        return rng >> 33U;
    };
    for (std::size_t i = 0; i < 20000; ++i) {
        auto const k = ic_key(next() % 3000);
        switch (next() % 4) {
        case 0:
        case 1:
            REQUIRE(t.try_emplace(k, i).second == ref.try_emplace(k, i).second);
            break;
        case 2:
            REQUIRE(t.erase(k) == ref.erase(k));
            break;
        default:
            REQUIRE(t.count(k) == ref.count(k));
            break;
        }
    }
    REQUIRE(t.size() == ref.size());
    for (auto const& [k, v] : ref) {
        REQUIRE(t.at(k) == v);
    }
    REQUIRE(t.verify(ud::verify_level::full));

    // copy, move, rehash, clear: every path that hands the index container around
    auto copy = t;
    REQUIRE(copy == t);
    auto moved = std::move(copy);
    REQUIRE(moved == t);
    moved.rehash(0);
    REQUIRE(moved == t);
    moved.clear();
    REQUIRE(moved.empty());
    REQUIRE(!moved.contains(ic_key(1)));

    // the owning load goes through resize, copy and check for a container without a fused copy
    auto values = t.values();
    auto blocks = std::vector<typename table_t::index_block>(t.index().data(), t.index().data() + t.index().size());
    auto loaded = table_t(std::move(values), {blocks.data(), blocks.size()}, ud::trust::checked);
    REQUIRE(loaded == t);
    loaded.try_emplace(ic_key(5000), 1);
    REQUIRE(loaded.verify(ud::verify_level::full));

    auto bad = blocks;
    for (auto& b : bad) {
        for (std::size_t lane = 0; lane < 16; ++lane) {
            if (b.m_fingerprints[lane] != 0) {
                b.m_index[lane] = 0; // every slot on value 0
            }
        }
    }
    auto values2 = t.values();
    REQUIRE_THROWS_AS(table_t(std::move(values2), {bad.data(), bad.size()}, ud::trust::checked), std::invalid_argument);
    auto values3 = t.values();
    auto unchecked = table_t(std::move(values3), {bad.data(), bad.size()}, ud::trust::unchecked);
    REQUIRE(unchecked.size() == t.size());
}
