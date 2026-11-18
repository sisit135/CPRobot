// SPDX-License-Identifier: GPL-3.0-only

#include "localization/hal_sensors.hpp"
#include "Arduino.h"
#include "Wire.h"
#include "hal/sensors_config.hpp"
#include "logging/log.hpp"
#include "platform/hal_platform.hpp"
#include "remote/settings.hpp"
#include <algorithm>

namespace {
constexpr uint8_t address = board::sensors::kTcs34725Address7bit,
                  atime = 0xF6; // 10 cycles: nominal 24 ms.
constexpr int64_t integration_wait_us =
    30000; // Includes init/tolerance margin.
int64_t ready_us = 0;
uint32_t sequence = 0;
enum class Phase { FAULT, POWER_WAIT, INTEGRATING, IDLE_WAIT };
Phase phase = Phase::FAULT;
nav::EndTapeDetector tape({board::sensors::kTapeBlackMaxClear,
                           board::sensors::kTapeWhiteMinClear});
/** @brief Write one TCS34725 register using the Arduino I2C adapter.
 * @param[in] reg Register address.
 * @param[in] value Byte value.
 * @returns true if the bus acknowledged the transaction.
 */
bool write_reg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(uint8_t(0x80 | reg));
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
/** @brief Read auto-incrementing bytes from the TCS34725.
 * @param[in] reg First register.
 * @param[out] data Destination with room for count bytes.
 * @param[in] count Number of bytes, at most eight.
 * @returns true on a complete transaction.
 */
bool read_regs(uint8_t reg, uint8_t *data, uint8_t count) {
  Wire.beginTransmission(address);
  Wire.write(uint8_t(0xA0 | reg));
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom(address, count) != count)
    return false;
  for (uint8_t i = 0; i < count; ++i) {
    if (!Wire.available())
      return false;
    data[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}
} // namespace
bool hal_sensors_init() {
  tape = nav::EndTapeDetector(
      {board::sensors::kTapeBlackMaxClear, board::sensors::kTapeWhiteMinClear});
  phase = Phase::FAULT;
  if (!hal_board_sensor_buses_init())
    return false;
  if (!hal_board_sensors_init())
    return false;
  robot_log::write(robot_log::Level::INFO, "tcs", "init started");
  uint8_t id = 0;
  if (!read_regs(0x12, &id, 1) || id != 0x44 || !write_reg(0x00, 0) ||
      !write_reg(0x01, atime) || !write_reg(0x0F, 1) || !write_reg(0x00, 1)) {
    robot_log::write(robot_log::Level::ERROR, "tcs",
                     "health check/init failed id=0x%02x", id);
    return false; // Gain 4x, PON.
  }
  phase = Phase::POWER_WAIT;
  ready_us = hal_time_us() + 3000;
  robot_log::write(robot_log::Level::INFO, "tcs",
                   "hw configured; power settling");
  return true;
}
bool hal_tcs34725_initialized() { return phase != Phase::FAULT; }
bool hal_tcs34725_read(loc::RgbcSample *out, int64_t *stamp) {
  if (!out || !stamp || phase == Phase::FAULT || hal_time_us() < ready_us)
    return false;
  if (phase == Phase::POWER_WAIT || phase == Phase::IDLE_WAIT) {
    if (!write_reg(0x00, 3)) {
      phase = Phase::FAULT;
      return false;
    } // PON+AEN.
    phase = Phase::INTEGRATING;
    ready_us = hal_time_us() + integration_wait_us;
    return false;
  }
  uint8_t status = 0, bytes[8]{};
  if (!read_regs(0x13, &status, 1)) {
    phase = Phase::FAULT;
    return false;
  }
  if (!(status & 1)) { // No complete integration: bounded retry, never replay.
    if (hal_time_us() - ready_us > 10000)
      phase = Phase::FAULT;
    return false;
  }
  const bool ok = read_regs(0x14, bytes, 8);
  const bool stopped = write_reg(0x00, 1); // Stop integration until next poll.
  if (!ok || !stopped) {
    phase = Phase::FAULT;
    return false;
  }
  phase = Phase::IDLE_WAIT;
  ready_us = hal_time_us() + 3000;
  if (++sequence == 0) {
    phase = Phase::FAULT;
    return false;
  }
  loc::RgbcSample raw{};
  raw.clear = uint16_t(bytes[0] | uint16_t(bytes[1]) << 8);
  raw.red = uint16_t(bytes[2] | uint16_t(bytes[3]) << 8);
  raw.green = uint16_t(bytes[4] | uint16_t(bytes[5]) << 8);
  raw.blue = uint16_t(bytes[6] | uint16_t(bytes[7]) << 8);
  raw.sequence = sequence;
  // Conservative 75% of digital saturation for short integration.
  constexpr uint16_t limit = (256 - atime) * 1024 * 3 / 4;
  raw.saturated = std::max({raw.clear, raw.red, raw.green, raw.blue}) >= limit;
  *out = raw;
  *stamp = hal_time_us();
  return true;
}
bool hal_color_classify(const loc::RgbcSample &raw, loc::MarkerSample *out) {
  (void)raw;
  (void)out;
  return false; // All tiles are white: no start/Qtype/unique cell/line IDs.
}

bool hal_color_marker_read(loc::MarkerSample *out, int64_t *stamp) {
  loc::RgbcSample raw{};
  return out && stamp && hal_tcs34725_read(&raw, stamp) &&
         hal_color_classify(raw, out);
}

bool hal_end_tape_classify(const loc::RgbcSample &raw, int64_t stamp,
                           nav::EndTapeEvent *out) {
  static uint16_t black = board::sensors::kTapeBlackMaxClear,
                  white = board::sensors::kTapeWhiteMinClear;
  uint16_t next_black, next_white;
  if (remote::tape_thresholds(&next_black, &next_white) &&
      (black != next_black || white != next_white)) {
    black = next_black;
    white = next_white;
    tape = nav::EndTapeDetector({black, white}); // Sole sensor-task owner.
  }
  return tape.process(raw, stamp, out);
}
