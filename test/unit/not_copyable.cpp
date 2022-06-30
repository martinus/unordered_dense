#include <ankerl/unordered_dense.h>

#include <doctest.h>

// not copyable, but movable.
class NoCopy {
public:
    NoCopy() noexcept = default;
    explicit NoCopy(size_t d) noexcept
        : mData(d) {}

    ~NoCopy() = default;
    NoCopy(NoCopy const&) = delete;
    auto operator=(NoCopy const&) -> NoCopy& = delete;

    NoCopy(NoCopy&&) = default;
    auto operator=(NoCopy&&) -> NoCopy& = default;

    [[nodiscard]] auto data() const -> size_t {
        return mData;
    }

private:
    size_t mData{};
};

TEST_CASE("not_copyable") {
    using Map = ankerl::unordered_dense::map<size_t, NoCopy>;

    // it's ok because it is movable.
    Map m;
    for (size_t i = 0; i < 100; ++i) {
        m[i];
        m.emplace(std::piecewise_construct, std::forward_as_tuple(i * 100), std::forward_as_tuple(i));
    }
    REQUIRE(m.size() == 199);

    // not copyable, because m is not copyable!
    // Map m2 = m;

    // movable works
    Map m2 = std::move(m);
    REQUIRE(m2.size() == 199);
    m = Map{};
    REQUIRE(m.size() == 0);
}
