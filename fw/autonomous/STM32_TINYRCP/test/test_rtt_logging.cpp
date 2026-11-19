// SPDX-License-Identifier: GPL-3.0-only

#include "hal/rtt_log.hpp"
#include "logging/log.hpp"
#include <cassert>
#include <cstring>
#include <string>

namespace {
// Simulate OpenOCD consuming channel 0 and publishing its read offset.
std::string host_read() {
  auto &up = _SEGGER_RTT.up[0];
  std::string text;
  while (up.read_offset != up.write_offset) {
    text += up.buffer[up.read_offset];
    up.read_offset = (up.read_offset + 1) % up.size;
  }
  return text;
}
} // namespace

int main() {
  assert(std::strcmp(_SEGGER_RTT.id, "SEGGER RTT") == 0);
  assert(_SEGGER_RTT.up_count == 1 && _SEGGER_RTT.down_count == 1);
  auto &up = _SEGGER_RTT.up[0];
  assert(up.flags == 0 && up.size == 2048);
  const std::string fill(up.size - 1, 'x');
  assert(hal_log_rtt_write(fill.data(), fill.size()));
  const uint32_t old_write = up.write_offset;
  assert(!hal_log_rtt_write("full\n", 5));
  assert(up.write_offset == old_write); // No partial record or overwrite.
  assert(hal_log_rtt_dropped() == 1);
  assert(host_read() == fill); // No host needed until now.

  up.read_offset = up.write_offset = up.size - 4;
  assert(hal_log_rtt_write("wraparound\n", 11));
  assert(up.write_offset == 7);
  assert(host_read() == "wraparound\n");
  up.read_offset = up.size; // Invalid debugger offset must not corrupt memory.
  assert(!hal_log_rtt_write("bad\n", 4));
  up.read_offset = up.write_offset;
  assert(!hal_log_rtt_write(fill.data(), up.size));
  assert(hal_log_rtt_dropped() == 3);

  assert(robot_log::start());
  assert(robot_log::enabled_sinks() & robot_log::RTT);
  unsigned serial = 0;
  robot_log::set_sink(
      robot_log::UART,
      [](const robot_log::Record &, void *p) {
        ++*static_cast<unsigned *>(p);
        return true;
      },
      &serial);
  assert(robot_log::write(robot_log::Level::ERROR, "bup", "test failure"));
  assert(robot_log::drain() == 1);
  assert(host_read().find("0 bup: test failure\n") != std::string::npos);
  assert(serial == 1);

  // A full RTT ring cannot prevent delivery to the existing sinks.
  assert(hal_log_rtt_write(fill.data(), fill.size()));
  assert(robot_log::write(robot_log::Level::INFO, "test", "still delivered"));
  assert(robot_log::drain() == 1);
  assert(serial == 2);
  assert(hal_log_rtt_dropped() == 4);
  assert(host_read() == fill);

  robot_log::configure(robot_log::Level::TRACE, robot_log::UART);
  assert(robot_log::write(robot_log::Level::INFO, "test", "RTT disabled"));
  assert(robot_log::drain() == 1 && host_read().empty());
  assert(serial == 3);

  unsigned custom_rtt = 0;
  assert(robot_log::set_sink(
      robot_log::RTT,
      [](const robot_log::Record &, void *p) {
        ++*static_cast<unsigned *>(p);
        return true;
      },
      &custom_rtt));
  robot_log::configure(robot_log::Level::INFO, robot_log::RTT);
  assert(robot_log::write(robot_log::Level::INFO, "test", "custom sink"));
  assert(robot_log::drain() == 1 && custom_rtt == 1);
  assert(serial == 3 && host_read().empty());
}
