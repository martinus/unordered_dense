#include <ankerl/unordered_dense.h>
#include <app/Counter.h>
#include <app/robin_hood.h>

#include "Provider.h"

#if defined(FUZZ)
#    define REQUIRE(x) ::fuzz::Provider::require(x)
#else
#    include <doctest.h>
#endif

#include <fmt/format.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace fuzz {

void api(uint8_t const* data, size_t size) {
    auto p = fuzz::Provider(data, size);
    Counter counts;

    using Map = ankerl::unordered_dense::map<Counter::Obj, Counter::Obj>;
    // using Map = robin_hood::unordered_flat_map<Counter::Obj, Counter::Obj>;
    auto map = Map();
    p.repeat_oneof(
        [&] {
            auto key = p.integral<size_t>();
            auto it = map.try_emplace(Counter::Obj(key, counts), Counter::Obj(key, counts)).first;
            REQUIRE(it != map.end());
            REQUIRE(it->first.get() == key);
        },
        [&] {
            auto key = p.integral<size_t>();
            map.emplace(std::piecewise_construct, std::forward_as_tuple(key, counts), std::forward_as_tuple(key + 77, counts));
        },
        [&] {
            auto key = p.integral<size_t>();
            map[Counter::Obj(key, counts)] = Counter::Obj(key + 123, counts);
        },
        [&] {
            auto key = p.integral<size_t>();
            map.insert(std::pair<Counter::Obj, Counter::Obj>(Counter::Obj(key, counts), Counter::Obj(key, counts)));
        },
        [&] {
            auto key = p.integral<size_t>();
            map.insert_or_assign(Counter::Obj(key, counts), Counter::Obj(key + 1, counts));
        },
        [&] {
            auto key = p.integral<size_t>();
            map.erase(Counter::Obj(key, counts));
        },
        [&] {
            map = Map{};
        },
        [&] {
            auto m = Map{};
            m.swap(map);
        },
        [&] {
            map.clear();
        },
        [&] {
            auto s = p.bounded<size_t>(1025);
            map.rehash(s);
        },
        [&] {
            auto s = p.bounded<size_t>(1025);
            map.reserve(s);
        },
        [&] {
            auto key = p.integral<size_t>();
            auto it = map.find(Counter::Obj(key, counts));
            auto d = std::distance(map.begin(), it);
            REQUIRE(0 <= d);
            REQUIRE(d <= static_cast<std::ptrdiff_t>(map.size()));
        },
        [&] {
            if (!map.empty()) {
                auto idx = p.bounded(static_cast<int>(map.size()));
                auto it = map.cbegin() + idx;
                auto const& key = it->first;
                auto found_it = map.find(key);
                REQUIRE(it == found_it);
            }
        },
        [&] {
            if (!map.empty()) {
                auto it = map.begin() + p.bounded(static_cast<int>(map.size()));
                map.erase(it);
            }
        },
        [&] {
            auto tmp = Map();
            std::swap(tmp, map);
        },
        [&] {
            map = std::initializer_list<std::pair<Counter::Obj, Counter::Obj>>{
                {{1, counts}, {2, counts}},
                {{3, counts}, {4, counts}},
                {{5, counts}, {6, counts}},
            };
            REQUIRE(map.size() == 3);
        },
        [&] {
            auto first_idx = 0;
            auto last_idx = 0;
            if (!map.empty()) {
                first_idx = p.bounded(static_cast<int>(map.size()));
                last_idx = p.bounded(static_cast<int>(map.size()));
                if (first_idx > last_idx) {
                    std::swap(first_idx, last_idx);
                }
            }
            map.erase(map.cbegin() + first_idx, map.cbegin() + last_idx);
        },
        [&] {
            map.~Map();
            counts.check_all_done();
            new (&map) Map();
        },
        [&] {
            std::erase_if(map, [&](Map::value_type const& /*v*/) {
                return p.integral<bool>();
            });
        });
}

} // namespace fuzz

#if defined(FUZZ)
extern "C" auto LLVMFuzzerTestOneInput(uint8_t const* data, size_t size) -> int {
    fuzz::api(data, size);
    return 0;
}
#endif

#if 0

#    include <cassert>
#    include <chrono>
#    include <filesystem>
#    include <fstream>

__attribute__((weak)) extern auto LLVMFuzzerInitialize(int* argc, char*** argv) -> int;

class Periodic {
    std::chrono::steady_clock::time_point m_next{};
    std::chrono::steady_clock::duration m_interval{};

public:
    Periodic(std::chrono::steady_clock::duration interval)
        : m_interval(interval) {}

    operator bool() {
        auto now = std::chrono::steady_clock::now();
        if (now < m_next) {
            return false;
        }
        m_next = now + m_interval;
        return true;
    }
};

class ProgressBar {
    size_t m_width;
    size_t m_total;
    std::vector<std::string> m_symbols;

    static auto split(std::string_view symbols, char sep) -> std::vector<std::string> {
        auto s = std::vector<std::string>();
        while (true) {
            auto idx = symbols.find(sep);
            if (idx == std::string_view::npos) {
                break;
            }
            s.emplace_back(symbols.substr(0, idx));
            symbols.remove_prefix(idx + 1);
        }
        s.emplace_back(symbols);
        return s;
    }

public:
    ProgressBar(size_t width, size_t total, std::string_view symbols = "⡀ ⡄ ⡆ ⡇ ⡏ ⡟ ⡿ ⣿")
        : m_width(width)
        , m_total(total)
        , m_symbols(split(symbols, ' ')) {}

    auto operator()(size_t current) const -> std::string {
        auto const total_states = m_width * m_symbols.size() + 1;
        auto const current_state = total_states * current / m_total;
        std::string str;
        auto num_full = std::min(m_width, current_state / m_symbols.size());
        for (size_t i = 0; i < num_full; ++i) {
            str += m_symbols.back();
        }

        if (num_full < m_width) {
            auto remaining = current_state - num_full * m_symbols.size();
            if (0U != remaining) {
                str += m_symbols[remaining - 1];
            }

            auto num_fillers = m_width - num_full - (0U == remaining ? 0 : 1);
            for (size_t i = 0; i < num_fillers; ++i) {
                str.push_back(' ');
            }
        }
        return str;
    }
};

auto main(int argc, char** argv) -> int {
    using namespace std::literals;

    if (nullptr != LLVMFuzzerInitialize) {
        LLVMFuzzerInitialize(&argc, &argv);
    }

    auto total_files = 0;
    for (int i = 1; i < argc; ++i) {
        auto dir = std::filesystem::path(argv[i]);
        auto it = std::filesystem::directory_iterator(dir);
        total_files += std::distance(begin(it), end(it));
    }

    auto log = Periodic(200ms);
    auto const pb = ProgressBar(50, total_files);

    auto num_files = size_t();
    for (int i = 1; i < argc; ++i) {
        auto dir = std::filesystem::path(argv[i]);
        for (auto const& dir_entry : std::filesystem::directory_iterator(dir)) {
            ++num_files;
            auto const& path = dir_entry.path();
            auto f = std::ifstream(path);
            auto content = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            LLVMFuzzerTestOneInput(reinterpret_cast<uint8_t const*>(content.data()), content.size());

            if (log) {
                fmt::print(stderr, "\r|{}| {:7}/{:7}  ", pb(num_files), num_files, total_files);
            }
        }
    }
    fmt::print(stderr, "\r|{}| {:7}/{:7}  \n", pb(num_files), num_files, total_files);
}

#endif