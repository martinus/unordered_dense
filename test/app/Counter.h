#pragma once

#include <fmt/core.h>
#include <fmt/format.h>

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <string_view>

class CountObj;

struct Counter {
    // Obj for only swaps & equals. Used for optimizing.
    // Can't use static counters here because I want to do it in parallel.
    class Obj {
    public:
        // required for operator[]
        Obj();
        Obj(const size_t& data, Counter& counts);
        Obj(const Obj& o);
        Obj(Obj&& o) noexcept;
        ~Obj();

        auto operator==(const Obj& o) const -> bool;
        auto operator<(const Obj& o) const -> bool;
        auto operator=(const Obj& o) -> Obj&;
        auto operator=(Obj&& o) noexcept -> Obj&;

        [[nodiscard]] auto get() const -> size_t const&;
        auto get() -> size_t&;

        void swap(Obj& other);
        [[nodiscard]] auto getForHash() const -> size_t;

    private:
        size_t mData;
        Counter* mCounts;
    };

    Counter();
    ~Counter();

    size_t ctor{};
    size_t defaultCtor{};
    size_t copyCtor{};
    size_t dtor{};
    size_t equals{};
    size_t less{};
    size_t assign{};
    size_t swaps{};
    size_t get{};
    size_t constGet{};
    size_t hash{};
    size_t moveCtor{};
    size_t moveAssign{};

    std::string m_records =
        "\n     ctor  defctor  cpyctor     dtor   assign    swaps      get  cnstget     hash   equals     less   ctormv assignmv|   total |\n";

    void operator()(std::string_view title);

    [[nodiscard]] auto total() const -> size_t;

    static size_t staticDefaultCtor; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    static size_t staticDtor;        // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

// Throws an exception, this overload should never be taken!
inline auto operator new(size_t s, Counter::Obj* ptr) -> void*;

auto operator<<(std::ostream& os, Counter const& c) -> std::ostream&;

namespace std {

template <>
struct hash<Counter::Obj> {
    [[nodiscard]] auto operator()(const Counter::Obj& c) const noexcept -> size_t {
        return hash<size_t>{}(c.getForHash());
    }
};

} // namespace std

template <>
struct fmt::formatter<Counter::Obj> {
    static constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.end();
    }
    static auto format(Counter::Obj const& o, fmt::format_context& ctx) -> decltype(ctx.out()) {
        return format_to(ctx.out(), "{}", o.get());
    }
};
