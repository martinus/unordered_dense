#pragma once

#include <cstddef>
#include <cstdint>

namespace fuzz {

void string_map(uint8_t const* data, size_t size);
void string_segmented_map(uint8_t const* data, size_t size);
void string_deque_map(uint8_t const* data, size_t size);

} // namespace fuzz
