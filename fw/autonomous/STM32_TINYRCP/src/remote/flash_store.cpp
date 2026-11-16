// SPDX-License-Identifier: GPL-3.0-only
// STM32F411CE only: firmware below 0x08040000; littlefs sectors 6 and 7.
#include "remote/board_store.hpp"
#include "remote/flash_layout.hpp"
#include <Arduino.h>
#include <cstring>
extern "C" uint8_t _sidata, _sdata, _edata;
namespace remote {
namespace {
using namespace flash_layout;
lfs_t fs{};
Store store(fs);
alignas(4) uint8_t read_cache[cache_size], prog_cache[cache_size], lookahead[8];
bool initialized = false;
int read(const lfs_config *, lfs_block_t b, lfs_off_t off, void *out,
         lfs_size_t n) {
  if (b >= blocks || off > block_size || n > block_size - off)
    return LFS_ERR_IO;
  std::memcpy(out, reinterpret_cast<const void *>(base + b * block_size + off),
              n);
  return 0;
}
int prog(const lfs_config *, lfs_block_t b, lfs_off_t off, const void *in,
         lfs_size_t n) {
  if (b >= blocks || off > block_size || n > block_size - off || (off % 4) ||
      (n % 4))
    return LFS_ERR_IO;
  if (HAL_FLASH_Unlock() != HAL_OK)
    return LFS_ERR_IO;
  auto p = static_cast<const uint8_t *>(in);
  int result = 0;
  for (lfs_size_t i = 0; i < n; i += 4) {
    uint32_t word;
    std::memcpy(&word, p + i, 4);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                          base + b * block_size + off + i, word) != HAL_OK) {
      result = LFS_ERR_IO;
      break;
    }
  }
  HAL_FLASH_Lock();
  FLASH_FlushCaches();
  return result;
}
int erase(const lfs_config *, lfs_block_t b) {
  if (b >= blocks)
    return LFS_ERR_IO;
  if (HAL_FLASH_Unlock() != HAL_OK)
    return LFS_ERR_IO;
  FLASH_EraseInitTypeDef e{};
  e.TypeErase = FLASH_TYPEERASE_SECTORS;
  e.Sector = FLASH_SECTOR_6 + b;
  e.NbSectors = 1;
  e.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  uint32_t error = 0;
  auto rc = HAL_FLASHEx_Erase(&e, &error);
  HAL_FLASH_Lock();
  return rc == HAL_OK ? 0 : LFS_ERR_IO;
}
int sync(const lfs_config *) {
  __DSB();
  __ISB();
  return 0;
}
bool erased() {
  auto p = reinterpret_cast<const volatile uint32_t *>(base);
  for (std::size_t i = 0; i < blocks * block_size / 4; ++i)
    if (p[i] != 0xffffffffu)
      return false;
  return true;
}
lfs_config cfg{};
} // namespace
bool board_init() {
  // Reject a 256 KiB F411CC part; never erase based on a board-name assumption.
  if (*reinterpret_cast<const volatile uint16_t *>(FLASHSIZE_BASE) < 512)
    return false;
  const auto data_load = reinterpret_cast<uintptr_t>(&_sidata);
  const auto data_size = reinterpret_cast<uintptr_t>(&_edata) -
                         reinterpret_cast<uintptr_t>(&_sdata);
  if (data_load < 0x08000000u || data_load > base ||
      data_size > base - data_load)
    return false; // A mismatched linker layout must never erase application
                  // data.
  cfg.read = read;
  cfg.prog = prog;
  cfg.erase = erase;
  cfg.sync = sync;
  cfg.read_size = 4;
  cfg.prog_size = 4;
  cfg.block_size = block_size;
  cfg.block_count = blocks;
  cfg.block_cycles = 100;
  cfg.cache_size = cache_size;
  cfg.lookahead_size = 8;
  cfg.read_buffer = read_cache;
  cfg.prog_buffer = prog_cache;
  cfg.lookahead_buffer = lookahead;
  cfg.inline_max = file_max;
  cfg.file_max = file_max;
  cfg.name_max = name_max;
  cfg.metadata_max = metadata_max;
  int rc = lfs_mount(&fs, &cfg);
  if (rc < 0) {
    if (!erased() || lfs_format(&fs, &cfg) < 0 || lfs_mount(&fs, &cfg) < 0)
      return false;
  }
  initialized = store.init() == OK;
  if (initialized)
    apply(store);
  return initialized;
}
Store *board_store() { return initialized && store.ready() ? &store : nullptr; }
} // namespace remote
