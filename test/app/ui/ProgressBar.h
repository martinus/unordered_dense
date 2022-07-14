#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ui {

class ProgressBar {
    size_t m_width;
    size_t m_total;
    std::vector<std::string> m_symbols;

public:
    ProgressBar(size_t width, size_t total, std::string_view symbols = "⡀ ⡄ ⡆ ⡇ ⡏ ⡟ ⡿ ⣿");

    auto operator()(size_t current) const -> std::string;
};

} // namespace ui
