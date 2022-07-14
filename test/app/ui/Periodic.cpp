#include "Periodic.h"

namespace ui {

Periodic::Periodic(std::chrono::steady_clock::duration interval)
    : m_interval(interval) {}

Periodic::operator bool() {
    auto now = std::chrono::steady_clock::now();
    if (now < m_next) {
        return false;
    }
    m_next = now + m_interval;
    return true;
}

} // namespace ui
