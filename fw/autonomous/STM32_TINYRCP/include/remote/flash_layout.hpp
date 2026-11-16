// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
namespace remote::flash_layout {
inline constexpr uint32_t base = 0x08040000u;
inline constexpr uint32_t block_size = 128 * 1024, blocks = 2;
inline constexpr uint32_t cache_size = 256, metadata_max = 4096;
inline constexpr uint32_t file_max = 256, name_max = 16;
} // namespace remote::flash_layout
