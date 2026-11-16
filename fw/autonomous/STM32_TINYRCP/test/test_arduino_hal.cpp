// SPDX-License-Identifier: GPL-3.0-only

#include "Wire.h"
#include "localization/hal_sensors.hpp"
#include "logging/log.hpp"
#include "platform/hal_platform.hpp"
#include <cassert>
#include <cstdio>
FakeWire Wire;
uint32_t fake_us = 0;
unsigned long micros() { return fake_us; }
bool hal_board_sensors_init() { return true; }
namespace robot_log {
bool write(Level, const char *, const char *, ...) { return true; }
} // namespace robot_log
const loc::ColorReference *hal_color_references(std::size_t *count) {
  static const loc::ColorReference refs[] = {
      {0.8f, 0.1f, 0.1f, loc::MarkerGeometry::CELL, 1, 2}};
  *count = 1;
  return refs;
}
/** @brief Drive one fake conversion through the public nonblocking HAL.
 * @param[out] marker Event output, if emitted.
 * @returns Whether the third matching conversion emitted an entry event.
 */
bool conversion(loc::MarkerSample &marker) {
  int64_t stamp = 0;
  fake_us += 3000;
  assert(!hal_color_marker_read(&marker, &stamp)); // AEN starts, no wait.
  assert(Wire.registers[0] == 3);
  assert(!hal_color_marker_read(&marker,
                                &stamp)); // Same-time poll is not new data.
  fake_us += 30000;
  const bool emitted = hal_color_marker_read(&marker, &stamp);
  assert(Wire.registers[0] == 1); // Integration disabled after one read.
  if (emitted)
    assert(stamp == hal_time_us());
  return emitted;
}
int main() {
  Wire.registers[0x12] = 0x44;
  Wire.registers[0x13] = 1;
  const uint16_t values[] = {1200, 800, 100, 100};
  for (int j = 0; j < 4; ++j) {
    Wire.registers[0x14 + 2 * j] = values[j] & 255;
    Wire.registers[0x15 + 2 * j] = values[j] >> 8;
  }
  assert(hal_sensors_init());
  assert(Wire.registers[1] == 0xF6 && Wire.registers[0x0F] == 1 &&
         Wire.registers[0] == 1);
  loc::MarkerSample marker{};
  assert(!conversion(marker));
  assert(!conversion(marker));
  assert(!conversion(marker));
  assert(
      !marker.detected); // Even old calibrated color refs cannot invent a cell.
  assert(!conversion(marker));
  nav::EndTapeEvent tape{};
  auto optical = [&](uint16_t clear) {
    Wire.registers[0x14] = clear & 255;
    Wire.registers[0x15] = clear >> 8;
    loc::RgbcSample raw{};
    int64_t t = 0;
    fake_us += 3000;
    assert(!hal_tcs34725_read(&raw, &t));
    fake_us += 30000;
    assert(hal_tcs34725_read(&raw, &t));
    assert(raw.clear == clear);
    const bool event = hal_end_tape_classify(raw, t, &tape);
    assert(!hal_end_tape_classify(raw, t, &tape)); // No duplicate evidence.
    return event;
  };
  for (int i = 0; i < 3; ++i)
    assert(!optical(1200));
  assert(!optical(100));
  assert(!optical(100));
  assert(optical(100));
  assert(tape.id == 1 && tape.first_us < tape.confirmed_us);
  const auto first = tape.first_us, confirmed = tape.confirmed_us;
  assert(optical(100)); // Same continuously observed entry, fresh acquisition.
  assert(tape.id == 1 && tape.first_us == first &&
         tape.confirmed_us > confirmed);
  Wire.fail = true;
  fake_us += 3000;
  int64_t stamp = 0;
  assert(!hal_color_marker_read(&marker, &stamp)); // Latches bus fault.
  Wire.fail = false;
  fake_us += 100000;
  assert(!hal_color_marker_read(&marker, &stamp));
  fake_us = 0xfffffff0U;
  const int64_t before = hal_time_us();
  fake_us = 0x20;
  assert(hal_time_us() - before == 48); // 32-bit clock wrap.
  Wire.registers[0x12] = 0;
  assert(!hal_sensors_init()); // Wrong device ID.
  std::puts("PASS Arduino TCS register flow, byte order, conversion freshness, "
            "debounce, bus fault, clock wrap");
}
