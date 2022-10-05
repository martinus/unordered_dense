#pragma once

#include <cstddef>
#include <cstdint>

namespace fuzz {

void insert_erase_map(uint8_t const* data, size_t size);
void insert_erase_segmented_map(uint8_t const* data, size_t size);
void insert_erase_deque_map(uint8_t const* data, size_t size);

} // namespace fuzz
