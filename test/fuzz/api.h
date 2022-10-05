#pragma once

#include <cstddef>
#include <cstdint>

namespace fuzz {

void api_map(uint8_t const* data, size_t size);
void api_segmented_map(uint8_t const* data, size_t size);
void api_deque_map(uint8_t const* data, size_t size);

} // namespace fuzz
