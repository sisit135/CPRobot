// SPDX-License-Identifier: GPL-3.0-only

#include "hal/rtt_log.hpp"

#ifdef AXI_LOG_RTT_ENABLE
#include <atomic>
#include <cstring>

namespace {
alignas(4) char log_buffer[2048];
volatile uint32_t lost = 0;
} // namespace

// The debugger discovers this standard RTT symbol/ID. All storage is static;
// never reinitialize host offsets on attach, so early boot records survive.
extern "C" {
alignas(4) __attribute__((
    used, externally_visible)) rtt_log::ControlBlock _SEGGER_RTT = {
    "SEGGER RTT", 1, 0, {{"THS logs", log_buffer, sizeof log_buffer, 0, 0, 0}}};
}

#if defined(__arm__) || defined(__thumb__)
static_assert(sizeof(rtt_log::UpBuffer) == 24, "RTT upstream descriptor ABI");
static_assert(offsetof(rtt_log::ControlBlock, up) == 24, "RTT header ABI");
static_assert(sizeof(rtt_log::ControlBlock) == 48, "RTT control block ABI");
#endif

bool hal_log_rtt_write(const char *data, size_t size) {
  auto &channel = _SEGGER_RTT.up[0];
  const uint32_t write = channel.write_offset;
  const uint32_t read = channel.read_offset;
  std::atomic_thread_fence(std::memory_order_acquire);
  if (!data || !size || write >= channel.size || read >= channel.size ||
      size >= channel.size ||
      size > (read + channel.size - write - 1) % channel.size) {
    ++lost;
    return false;
  }
  const size_t first =
      size < channel.size - write ? size : channel.size - write;
  std::memcpy(channel.buffer + write, data, first);
  std::memcpy(channel.buffer, data + first, size - first);
  // Publish only after both portions are visible to debugger memory reads.
  std::atomic_thread_fence(std::memory_order_release);
  channel.write_offset = static_cast<uint32_t>((write + size) % channel.size);
  return true;
}

uint32_t hal_log_rtt_dropped() { return lost; }
#endif
