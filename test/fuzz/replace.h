#pragma once

#include <cstddef>
#include <cstdint>

namespace fuzz {

void replace_map(uint8_t const* data, size_t size);
void replace_segmented_map(uint8_t const* data, size_t size);
void replace_deque_map(uint8_t const* data, size_t size);

} // namespace fuzz
