#include <ankerl/unordered_dense.h>

#include <doctest.h>

class NoMove {
public:
    NoMove() noexcept = default;
    explicit NoMove(size_t d) noexcept
        : mData(d) {}
    ~NoMove() = default;

    NoMove(NoMove const&) = default;
    auto operator=(NoMove const&) -> NoMove& = default;

    NoMove(NoMove&&) = delete;
    auto operator=(NoMove&&) -> NoMove& = delete;

    [[nodiscard]] auto data() const -> size_t {
        return mData;
    }

private:
    size_t mData{};
};

// doesn't work with robin_hood::unordered_flat_map<size_t, NoMove> because not movable and not
// copyable
TEST_CASE("not_moveable") {
    using Map = ankerl::unordered_dense::map<size_t, NoMove>;

    // it's ok because it is movable.
    auto m = Map();
    for (size_t i = 0; i < 100; ++i) {
        m[i];
        m.emplace(std::piecewise_construct, std::forward_as_tuple(i * 100), std::forward_as_tuple(i));
    }
    REQUIRE(m.size() == 199);

    // not copyable, because m is not copyable!
    // Map m2 = m;

    // not movable
    // Map m2 = std::move(m);
    // REQUIRE(m2.size() == 199);
    m.clear();
    REQUIRE(m.size() == 0);
}
