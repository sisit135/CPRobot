// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "remote/config.hpp"
#include "remote/flash_layout.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace lifecycle {
// Real configuration codec and Store, with LittleFS blocks held only in RAM.
class MemoryStore {
  static constexpr auto block_size = remote::flash_layout::block_size;
  std::vector<uint8_t> bytes =
      std::vector<uint8_t>(block_size * remote::flash_layout::blocks, 255);
  lfs_t fs{};
  lfs_config cfg{};

public:
  remote::Store store{fs};
  MemoryStore() {
    cfg.context = this;
    cfg.read_size = cfg.prog_size = 4;
    cfg.block_size = block_size;
    cfg.block_count = remote::flash_layout::blocks;
    cfg.cache_size = remote::flash_layout::cache_size;
    cfg.lookahead_size = 8;
    cfg.block_cycles = 100;
    cfg.metadata_max = remote::flash_layout::metadata_max;
    cfg.inline_max = cfg.file_max = remote::flash_layout::file_max;
    cfg.name_max = remote::flash_layout::name_max;
    cfg.read = [](const lfs_config *c, lfs_block_t b, lfs_off_t off, void *out,
                  lfs_size_t n) {
      auto &s = *static_cast<MemoryStore *>(c->context);
      if (b >= c->block_count || off + n > c->block_size)
        return int(LFS_ERR_IO);
      std::memcpy(out, s.bytes.data() + b * block_size + off, n);
      return 0;
    };
    cfg.prog = [](const lfs_config *c, lfs_block_t b, lfs_off_t off,
                  const void *in, lfs_size_t n) {
      auto &s = *static_cast<MemoryStore *>(c->context);
      if (b >= c->block_count || off + n > c->block_size)
        return int(LFS_ERR_IO);
      auto p = static_cast<const uint8_t *>(in);
      for (lfs_size_t i = 0; i < n; ++i)
        s.bytes[b * block_size + off + i] &= p[i];
      return 0;
    };
    cfg.erase = [](const lfs_config *c, lfs_block_t b) {
      auto &s = *static_cast<MemoryStore *>(c->context);
      if (b >= c->block_count)
        return int(LFS_ERR_IO);
      std::fill_n(s.bytes.data() + b * block_size, block_size, uint8_t(255));
      return 0;
    };
    cfg.sync = [](const lfs_config *) { return 0; };
    if (lfs_format(&fs, &cfg) || lfs_mount(&fs, &cfg) ||
        store.init() != remote::OK)
      throw std::runtime_error("RAM configuration mount failed");
  }
  ~MemoryStore() { lfs_unmount(&fs); }
  void set(uint32_t key, float value) {
    if (store.write(key, value, store.snapshot().revision) != remote::OK)
      throw std::runtime_error("invalid simulation configuration");
  }
};
} // namespace lifecycle
