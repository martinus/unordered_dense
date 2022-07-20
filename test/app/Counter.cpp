#include <app/Counter.h>

#include <app/print.h> // for print

#include <cstdlib>       // for abort
#include <ostream>       // for ostream
#include <stdexcept>     // for runtime_error
#include <unordered_set> // for unordered_set
#include <utility>       // for swap, pair

#define COUNTER_ENABLE_UNORDERED_SET 1

#if COUNTER_ENABLE_UNORDERED_SET
auto singletonConstructedObjects() -> std::unordered_set<Counter::Obj const*>& {
    static std::unordered_set<Counter::Obj const*> data{};
    return data;
}
#endif

Counter::Obj::Obj()
    : mData(0)
    , mCounts(nullptr) {
#if COUNTER_ENABLE_UNORDERED_SET
    if (!singletonConstructedObjects().emplace(this).second) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
#endif
    ++staticDefaultCtor;
}

Counter::Obj::Obj(const size_t& data, Counter& counts)
    : mData(data)
    , mCounts(&counts) {
#if COUNTER_ENABLE_UNORDERED_SET
    if (!singletonConstructedObjects().emplace(this).second) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
#endif
    ++mCounts->ctor;
}

Counter::Obj::Obj(const Counter::Obj& o)
    : mData(o.mData)
    , mCounts(o.mCounts) {
#if COUNTER_ENABLE_UNORDERED_SET
    if (1 != singletonConstructedObjects().count(&o)) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
    if (!singletonConstructedObjects().emplace(this).second) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
#endif
    if (nullptr != mCounts) {
        ++mCounts->copyCtor;
    }
}

Counter::Obj::Obj(Counter::Obj&& o) noexcept
    : mData(o.mData)
    , mCounts(o.mCounts) {
#if COUNTER_ENABLE_UNORDERED_SET
    if (1 != singletonConstructedObjects().count(&o)) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
    if (!singletonConstructedObjects().emplace(this).second) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
#endif
    if (nullptr != mCounts) {
        ++mCounts->moveCtor;
    }
}

Counter::Obj::~Obj() {
#if COUNTER_ENABLE_UNORDERED_SET
    if (1 != singletonConstructedObjects().erase(this)) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
#endif
    if (nullptr != mCounts) {
        ++mCounts->dtor;
    } else {
        ++staticDtor;
    }
}

auto Counter::Obj::operator==(const Counter::Obj& o) const -> bool {
#if COUNTER_ENABLE_UNORDERED_SET
    if (1 != singletonConstructedObjects().count(this) || 1 != singletonConstructedObjects().count(&o)) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
#endif
    if (nullptr != mCounts) {
        ++mCounts->equals;
    }
    return mData == o.mData;
}

auto Counter::Obj::operator<(const Obj& o) const -> bool {
#if COUNTER_ENABLE_UNORDERED_SET
    if (1 != singletonConstructedObjects().count(this) || 1 != singletonConstructedObjects().count(&o)) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
#endif
    if (nullptr != mCounts) {
        ++mCounts->less;
    }
    return mData < o.mData;
}

// NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
auto Counter::Obj::operator=(const Counter::Obj& o) -> Counter::Obj& {
#if COUNTER_ENABLE_UNORDERED_SET
    if (1 != singletonConstructedObjects().count(this) || 1 != singletonConstructedObjects().count(&o)) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
#endif
    mCounts = o.mCounts;
    if (nullptr != mCounts) {
        ++mCounts->assign;
    }
    mData = o.mData;
    return *this;
}

auto Counter::Obj::operator=(Counter::Obj&& o) noexcept -> Counter::Obj& {
#if COUNTER_ENABLE_UNORDERED_SET
    if (1 != singletonConstructedObjects().count(this) || 1 != singletonConstructedObjects().count(&o)) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
#endif
    if (nullptr != o.mCounts) {
        mCounts = o.mCounts;
    }
    mData = o.mData;
    if (nullptr != mCounts) {
        ++mCounts->moveAssign;
    }
    return *this;
}

auto Counter::Obj::get() const -> size_t const& {
    if (nullptr != mCounts) {
        ++mCounts->constGet;
    }
    return mData;
}

auto Counter::Obj::get() -> size_t& {
    if (nullptr != mCounts) {
        ++mCounts->get;
    }
    return mData;
}

void Counter::Obj::swap(Obj& other) {
#if COUNTER_ENABLE_UNORDERED_SET
    if (1 != singletonConstructedObjects().count(this) || 1 != singletonConstructedObjects().count(&other)) {
        test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
        std::abort();
    }
#endif
    using std::swap;
    swap(mData, other.mData);
    swap(mCounts, other.mCounts);
    if (nullptr != mCounts) {
        ++mCounts->swaps;
    }
}

auto Counter::Obj::getForHash() const -> size_t {
    if (nullptr != mCounts) {
        ++mCounts->hash;
    }
    return mData;
}

Counter::Counter() {
    Counter::staticDefaultCtor = 0;
    Counter::staticDtor = 0;
}

void Counter::check_all_done() const {
#if COUNTER_ENABLE_UNORDERED_SET
    // check that all are destructed
    if (!singletonConstructedObjects().empty()) {
        test::print("ERROR at ~Counter(): got {} objects still alive!", singletonConstructedObjects().size());
        std::abort();
    }
    if (dtor + staticDtor != ctor + staticDefaultCtor + copyCtor + defaultCtor + moveCtor) {
        test::print("ERROR at ~Counter(): number of counts does not match!\n");
        test::print("{} dtor + {} staticDtor != {} ctor + {} staticDefaultCtor + {} copyCtor + {} defaultCtor + {} moveCtor\n",
                    dtor,
                    staticDtor,
                    ctor,
                    staticDefaultCtor,
                    copyCtor,
                    defaultCtor,
                    moveCtor);
        std::abort();
    }
#endif
}

Counter::~Counter() {
    check_all_done();
}

auto Counter::total() const -> size_t {
    return ctor + staticDefaultCtor + copyCtor + (dtor + staticDtor) + equals + less + assign + swaps + get + constGet + hash +
           moveCtor + moveAssign;
}

void Counter::operator()(std::string_view title) {
    m_records += fmt::format("{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}|{:9}| {}\n",
                             ctor,
                             staticDefaultCtor,
                             copyCtor,
                             dtor + staticDtor,
                             assign,
                             swaps,
                             get,
                             constGet,
                             hash,
                             equals,
                             less,
                             moveCtor,
                             moveAssign,
                             total(),
                             title);
}

auto operator<<(std::ostream& os, Counter const& c) -> std::ostream& {
    return os << c.m_records;
}

auto operator new(size_t /*unused*/, Counter::Obj* /*unused*/) -> void* {
    throw std::runtime_error("operator new overload is taken! Cast to void* to ensure the void pointer overload is taken.");
}
size_t Counter::staticDefaultCtor = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
size_t Counter::staticDtor = 0;        // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
