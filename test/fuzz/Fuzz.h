#pragma once

#include "FuzzedDataProvider.h"

class Fuzz {
    FuzzedDataProvider m_fdp;

public:
    inline explicit Fuzz(uint8_t const* data, size_t size)
        : m_fdp(data, size) {}

    // random number in inclusive range [min, max]
    template <typename T>
    auto range(T min, T max) -> T {
        return m_fdp.ConsumeIntegralInRange(min, max);
    }

    template <typename T>
    auto integral() -> T {
        return m_fdp.ConsumeIntegral<T>();
    }

    template <>
    auto integral<bool>() -> bool {
        return m_fdp.ConsumeBool();
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
    void loop_call_any(Ops&&... op) {
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
    void limited_loop_call_any(size_t min, size_t max, Ops&&... op) {
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

    static inline void require(bool b) {
        if (!b) {
            __builtin_trap();
        }
    }
};
