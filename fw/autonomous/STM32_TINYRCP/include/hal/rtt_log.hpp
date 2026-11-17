// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>

namespace rtt_log {
// OpenOCD RTT ABI: channel 0 logs/AT replies up and AT command bytes down.
struct UpBuffer {
  const char *name;
  char *buffer;
  uint32_t size;
  volatile uint32_t write_offset;
  volatile uint32_t read_offset;
  uint32_t flags;
};
struct ControlBlock {
  char id[16];
  uint32_t up_count;
  uint32_t down_count;
  UpBuffer up[1];
  UpBuffer down[1];
};
} // namespace rtt_log

extern "C" rtt_log::ControlBlock _SEGGER_RTT;

/** Copy one complete record without waiting. Caller serializes writers.
 * Host owns read_offset; firmware owns write_offset. Full/corrupt buffers drop
 * the whole record. No host connection or trace-register setup is required. */
bool hal_log_rtt_write(const char *data, size_t size);
// Same nonblocking whole-line write without counting a retry as a lost log.
bool hal_rtt_try_write(const char *data, size_t size);
// Single firmware reader; host owns write_offset, firmware owns read_offset.
int hal_rtt_read();
uint32_t hal_log_rtt_dropped();
