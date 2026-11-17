// SPDX-License-Identifier: GPL-3.0-only

#include "hal/rtt_log.hpp"

#ifdef AXI_LOG_RTT_ENABLE
#include <atomic>
#include <cstring>

namespace {
alignas(4) char log_buffer[2048];
alignas(4) char command_buffer[512];
volatile uint32_t lost = 0;
} // namespace

// The debugger discovers this standard RTT symbol/ID. All storage is static;
// never reinitialize host offsets on attach, so early boot records survive.
extern "C" {
alignas(4)
    __attribute__((used,
                   externally_visible)) rtt_log::ControlBlock _SEGGER_RTT = {
        "SEGGER RTT",
        1,
        1,
        {{"THS logs / AT", log_buffer, sizeof log_buffer, 0, 0, 0}},
        {{"THS AT commands", command_buffer, sizeof command_buffer, 0, 0, 0}}};
}

#if defined(__arm__) || defined(__thumb__)
static_assert(sizeof(rtt_log::UpBuffer) == 24, "RTT upstream descriptor ABI");
static_assert(offsetof(rtt_log::ControlBlock, up) == 24, "RTT header ABI");
static_assert(offsetof(rtt_log::ControlBlock, down) == 48, "RTT down ABI");
static_assert(sizeof(rtt_log::ControlBlock) == 72, "RTT control block ABI");
#endif

bool hal_rtt_try_write(const char *data, size_t size) {
  auto &channel = _SEGGER_RTT.up[0];
  const uint32_t write = channel.write_offset;
  const uint32_t read = channel.read_offset;
  std::atomic_thread_fence(std::memory_order_acquire);
  if (!data || !size || write >= channel.size || read >= channel.size ||
      size >= channel.size ||
      size > (read + channel.size - write - 1) % channel.size) {
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

bool hal_log_rtt_write(const char *data, size_t size) {
  if (hal_rtt_try_write(data, size))
    return true;
  ++lost;
  return false;
}

int hal_rtt_read() {
  auto &channel = _SEGGER_RTT.down[0];
  const uint32_t read = channel.read_offset;
  const uint32_t write = channel.write_offset;
  std::atomic_thread_fence(std::memory_order_acquire);
  if (read >= channel.size || write >= channel.size || read == write)
    return -1;
  const auto byte = static_cast<unsigned char>(channel.buffer[read]);
  std::atomic_thread_fence(std::memory_order_release);
  channel.read_offset = (read + 1) % channel.size;
  return byte;
}

uint32_t hal_log_rtt_dropped() { return lost; }
#endif
