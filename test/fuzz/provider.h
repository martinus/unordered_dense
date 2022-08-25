#pragma once

#include <third-party/FuzzedDataProvider.h>

#include <type_traits>

namespace fuzz {

// Helper to provide a little bit more convenient interface than FuzzedDataProvider itself
class provider {
    FuzzedDataProvider m_fdp;

public:
    inline explicit provider(uint8_t const* data, size_t size)
        : m_fdp(data, size) {}

    // random number in inclusive range [min, max]
    template <typename T>
    auto range(T min, T max) -> T {
        return m_fdp.ConsumeIntegralInRange<T>(min, max);
    }

    template <typename T>
    auto bounded(T max_exclusive) -> T {
        if (0 == max_exclusive) {
            return {};
        }
        return m_fdp.ConsumeIntegralInRange<T>(0, max_exclusive - 1);
    }

    template <typename T>
    auto integral() -> T {
        if constexpr (std::is_same_v<bool, T>) {
            return m_fdp.ConsumeBool();
        } else {
            return m_fdp.ConsumeIntegral<T>();
        }
    }

    inline auto string(size_t max_length) -> std::string {
        return m_fdp.ConsumeRandomLengthString(max_length);
    }

    template <typename... Args>
    auto pick(Args&&... args) -> std::common_type_t<decltype(args)...>& {
        static constexpr auto num_ops = sizeof...(args);

        auto idx = size_t{};
        auto const chosen_idx = m_fdp.ConsumeIntegralInRange<size_t>(0, num_ops - 1);
        std::common_type_t<decltype(args)...>* result = nullptr;
        ((idx++ == chosen_idx ? (result = &args, true) : false) || ...);
        return *result;
    }

    template <typename... Ops>
    void repeat_oneof(Ops&&... op) {
        static constexpr auto num_ops = sizeof...(op);

        do {
            if constexpr (num_ops == 1) {
                (op(), ...);
            } else {
                auto chosen_op_idx = range<size_t>(0, num_ops - 1);
                auto op_idx = size_t{};
                ((op_idx++ == chosen_op_idx ? op() : void()), ...);
            }
        } while (0 != m_fdp.remaining_bytes());
    }

    template <typename... Ops>
    void limited_repeat_oneof(size_t min, size_t max, Ops&&... op) {
        static constexpr auto num_ops = sizeof...(op);

        size_t const num_evaluations = m_fdp.ConsumeIntegralInRange(min, max);
        for (size_t i = 0; i < num_evaluations; ++i) {
            if constexpr (num_ops == 1) {
                (op(), ...);
            } else {
                auto chosen_op_idx = range<size_t>(0, num_ops - 1);
                auto op_idx = size_t{};
                ((op_idx++ == chosen_op_idx ? op() : void()), ...);
            }
            if (m_fdp.remaining_bytes() == 0) {
                return;
            }
        }
    }

    auto has_remaining_bytes() -> bool {
        return 0U != m_fdp.remaining_bytes();
    }

    static inline void require(bool b) {
        if (!b) {
            throw std::exception();
        }
    }
};

} // namespace fuzz
