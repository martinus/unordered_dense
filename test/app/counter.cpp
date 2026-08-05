#include <app/counter.h>

#include <app/print.h> // for print

#include <cstdlib>   // for abort
#include <ostream>   // for ostream
#include <stdexcept> // for runtime_error
#include <utility>   // for swap, pair

static inline constexpr bool counter_enable_liveness_checks = true;

// Liveness used to be a global std::unordered_set of the addresses of live objects, consulted by every operation.
// That works, but it hashes and probes on pointer values, so which buckets get touched depends on where the objects
// happen to land. The fuzz targets run thousands of inputs inside one process, where the heap state differs from one
// input to the next, so the same input took different paths through that set and produced different coverage each
// time. AFL++ reports it as "instability detected during calibration" and wastes effort re-calibrating; libFuzzer
// does not report it but gets the same noisy signal. Measured on fuzz_api: hundreds of instability warnings per
// minute, against zero for fuzz_insert_erase, the one target that does not use counter::obj.
//
// So each object carries its own liveness instead, and only the number alive is global -- a counter that is never
// keyed by an address and never probed.
auto singleton_num_alive() -> size_t& {
    static size_t static_data{};
    return static_data;
}

auto counter::obj::is_alive() const -> bool {
    return this == m_alive;
}

counter::obj::obj()
    : m_data(0)
    , m_counts(nullptr)
    , m_alive(this) {
    if constexpr (counter_enable_liveness_checks) {
        ++singleton_num_alive();
    }
    ++static_default_ctor;
}

counter::obj::obj(const size_t& data, counter& counts)
    : m_data(data)
    , m_counts(&counts)
    , m_alive(this) {
    if constexpr (counter_enable_liveness_checks) {
        ++singleton_num_alive();
    }
    ++m_counts->m_data.m_ctor;
}

counter::obj::obj(const counter::obj& o)
    : m_data(o.m_data)
    , m_counts(o.m_counts)
    , m_alive(this) {
    if constexpr (counter_enable_liveness_checks) {
        if (!o.is_alive()) {
            test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
            std::abort();
        }
        ++singleton_num_alive();
    }
    if (nullptr != m_counts) {
        ++m_counts->m_data.m_copy_ctor;
    }
}

counter::obj::obj(counter::obj&& o) noexcept
    : m_data(o.m_data)
    , m_counts(o.m_counts)
    , m_alive(this) {
    if constexpr (counter_enable_liveness_checks) {
        if (!o.is_alive()) {
            test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
            std::abort();
        }
        ++singleton_num_alive();
    }
    if (nullptr != m_counts) {
        ++m_counts->m_data.m_move_ctor;
    }
}

counter::obj::~obj() {
    if constexpr (counter_enable_liveness_checks) {
        // Catches destroying an object twice, and destroying something that was never constructed.
        if (!is_alive()) {
            test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
            std::abort();
        }
        m_alive = nullptr;
        --singleton_num_alive();
    }
    if (nullptr != m_counts) {
        ++m_counts->m_data.m_dtor;
    } else {
        ++static_dtor;
    }
}

auto counter::obj::operator==(obj const& o) const -> bool {
    if constexpr (counter_enable_liveness_checks) {
        if (!is_alive() || !o.is_alive()) {
            test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
            std::abort();
        }
    }
    if (nullptr != m_counts) {
        ++m_counts->m_data.m_equals;
    }
    return m_data == o.m_data;
}

auto counter::obj::operator<(obj const& o) const -> bool {
    if constexpr (counter_enable_liveness_checks) {
        if (!is_alive() || !o.is_alive()) {
            test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
            std::abort();
        }
    }
    if (nullptr != m_counts) {
        ++m_counts->m_data.m_less;
    }
    return m_data < o.m_data;
}

// NOLINTNEXTLINE(bugprone-unhandled-self-assignment,cert-oop54-cpp)
auto counter::obj::operator=(obj const& o) -> counter::obj& {
    if constexpr (counter_enable_liveness_checks) {
        if (!is_alive() || !o.is_alive()) {
            test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
            std::abort();
        }
    }
    m_counts = o.m_counts;
    if (nullptr != m_counts) {
        ++m_counts->m_data.m_assign;
    }
    m_data = o.m_data;
    return *this;
}

auto counter::obj::operator=(obj&& o) noexcept -> counter::obj& {
    if constexpr (counter_enable_liveness_checks) {
        if (!is_alive() || !o.is_alive()) {
            test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
            std::abort();
        }
    }
    if (nullptr != o.m_counts) {
        m_counts = o.m_counts;
    }
    m_data = o.m_data;
    if (nullptr != m_counts) {
        ++m_counts->m_data.m_move_assign;
    }
    return *this;
}

auto counter::obj::get() const -> size_t const& {
    if (nullptr != m_counts) {
        ++m_counts->m_data.m_const_get;
    }
    return m_data;
}

auto counter::obj::counts() -> counter& {
    return *m_counts;
}

auto counter::obj::get() -> size_t& {
    if (nullptr != m_counts) {
        ++m_counts->m_data.m_get;
    }
    return m_data;
}

void counter::obj::swap(obj& other) {
    if constexpr (counter_enable_liveness_checks) {
        if (!is_alive() || !other.is_alive()) {
            test::print("ERROR at {}({}): {}\n", __FILE__, __LINE__, __func__);
            std::abort();
        }
    }
    using std::swap;
    swap(m_data, other.m_data);
    swap(m_counts, other.m_counts);
    if (nullptr != m_counts) {
        ++m_counts->m_data.m_swaps;
    }
}

auto counter::obj::get_for_hash() const -> size_t {
    if (nullptr != m_counts) {
        ++m_counts->m_data.m_hash;
    }
    return m_data;
}

counter::counter() {
    counter::static_default_ctor = 0;
    counter::static_dtor = 0;
}

void counter::check_all_done() const {
    if constexpr (counter_enable_liveness_checks) {
        // check that all are destructed
        if (0 != singleton_num_alive()) {
            test::print("ERROR at ~counter(): got {} objects still alive!", singleton_num_alive());
            std::abort();
        }

        if (m_data.m_dtor + static_dtor !=
            m_data.m_ctor + static_default_ctor + m_data.m_copy_ctor + m_data.m_default_ctor + m_data.m_move_ctor) {
            test::print("ERROR at ~counter(): number of counts does not match!\n");
            test::print(
                "{} dtor + {} staticDtor != {} ctor + {} staticDefaultCtor + {} copyCtor + {} defaultCtor + {} moveCtor\n",
                m_data.m_dtor,
                static_dtor,
                m_data.m_ctor,
                static_default_ctor,
                m_data.m_copy_ctor,
                m_data.m_default_ctor,
                m_data.m_move_ctor);
            std::abort();
        }
    }
}

counter::~counter() {
    check_all_done();
}

auto counter::total() const -> size_t {
    return m_data.m_ctor + static_default_ctor + m_data.m_copy_ctor + (m_data.m_dtor + static_dtor) + m_data.m_equals +
           m_data.m_less + m_data.m_assign + m_data.m_swaps + m_data.m_get + m_data.m_const_get + m_data.m_hash +
           m_data.m_move_ctor + m_data.m_move_assign;
}

void counter::operator()(std::string_view title) {
    m_records += fmt::format("{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}{:9}|{:9}| {}\n",
                             m_data.m_ctor,
                             static_default_ctor,
                             m_data.m_copy_ctor,
                             m_data.m_dtor + static_dtor,
                             m_data.m_assign,
                             m_data.m_swaps,
                             m_data.m_get,
                             m_data.m_const_get,
                             m_data.m_hash,
                             m_data.m_equals,
                             m_data.m_less,
                             m_data.m_move_ctor,
                             m_data.m_move_assign,
                             total(),
                             title);
}

auto operator<<(std::ostream& os, counter const& c) -> std::ostream& {
    return os << c.m_records;
}

auto operator new(size_t /*unused*/, counter::obj* /*unused*/) -> void* {
    throw std::runtime_error("operator new overload is taken! Cast to void* to ensure the void pointer overload is taken.");
}
size_t counter::static_default_ctor = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
size_t counter::static_dtor = 0;         // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
