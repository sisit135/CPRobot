// SPDX-License-Identifier: GPL-3.0-only
#include "hal/i2c_diagnostics.hpp"
#include "logging/log.hpp"
#include "platform/rtos.hpp"
#include <Arduino.h>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {
int64_t now = 1;
int64_t probe_cost = 100;
HAL_StatusTypeDef failure = HAL_ERROR;
std::vector<unsigned> addresses;
std::string output;
bool all_ack = false;
constexpr int sda = 10, scl = 11;
bool data_high = true, clock_high = true, active = false;
bool stuck_data = false, stretch = false;
unsigned bits = 0, byte = 0, starts = 0, stops = 0, clock_edges = 0;
int modes[2] = {INPUT, INPUT};
std::vector<unsigned> gpio_addresses;
std::string expected_source = "i2c1";
void reset() {
  now = 1;
  probe_cost = 100;
  failure = HAL_ERROR;
  all_ack = false;
  addresses.clear();
  output.clear();
  data_high = clock_high = true;
  active = stuck_data = stretch = false;
  bits = byte = starts = stops = clock_edges = 0;
  gpio_addresses.clear();
}
} // namespace
void pinMode(int pin, int mode) {
  assert(pin == sda || pin == scl);
  assert(mode == INPUT || mode == OUTPUT_OPEN_DRAIN);
  modes[pin == sda ? 0 : 1] = mode;
}
void delayMicroseconds(unsigned int us) { now += us; }
int digitalRead(int pin) {
  assert(pin == sda || pin == scl);
  if (pin == scl)
    return clock_high && !(stretch && active);
  if (stuck_data)
    return LOW;
  // The ninth clock is ACK only for 0x19 and 0x1e.
  if (active && bits == 9)
    return byte == (0x19 << 1) || byte == (0x1e << 1) ? LOW : HIGH;
  return data_high;
}
void digitalWrite(int pin, int value) {
  assert(pin == sda || pin == scl);
  if (pin == sda) {
    if (clock_high && !(stretch && active) && data_high && value == LOW) {
      assert(!active);
      active = true;
      bits = byte = 0;
      ++starts;
    } else if (clock_high && !stretch && !data_high && value == HIGH &&
               active) {
      assert(bits == 10); // Eight address clocks, ACK, STOP clock.
      active = false;
      ++stops;
    }
    data_high = value;
  } else {
    if (!clock_high && value == HIGH && active) {
      ++clock_edges;
      if (++bits <= 8)
        byte = (byte << 1) | unsigned(data_high);
      if (bits == 9) {
        assert(data_high); // Master releases ACK; it never drives this bit.
        assert((byte & 1) == 0); // Address-only write, never read device data.
        gpio_addresses.push_back(byte >> 1);
      }
    }
    clock_high = value;
  }
}
int64_t hal_time_us() { return now; }
void vTaskDelay(TickType_t ticks) { now += ticks * 1000; }
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *, uint16_t address,
                                        uint32_t trials, uint32_t timeout) {
  assert(trials == 1 && timeout == 2);
  assert((address & 1) == 0);
  assert(address >= (0x08 << 1) && address <= (0x77 << 1));
  addresses.push_back(address >> 1);
  now += probe_cost;
  return all_ack || address == (0x68 << 1) || address == (0x42 << 1) ? HAL_OK
                                                                     : failure;
}
bool robot_log::write(Level level, const char *source, const char *format,
                      ...) {
  assert(level == Level::INFO && std::string(source) == expected_source);
  char line[96];
  va_list args;
  va_start(args, format);
  const int length = std::vsnprintf(line, sizeof line, format, args);
  va_end(args);
  assert(length >= 0 && length < int(sizeof line));
  output += std::string(line) + '\n';
  return true;
}
int main() {
  I2C_HandleTypeDef bus{I2C1};
  const i2c_diagnostics::Binding bindings[] = {{0x68, "MPU9250 accel/gyro"},
                                               {0x29, "TCS34725 color"}};
  const auto scan = [&] {
    i2c_diagnostics::scan(&bus, "main", "before init", bindings, 2);
  };
  scan();
  assert(addresses.size() == 112);
  assert(output.find("I2C1 / main / before init") != std::string::npos);
  assert(output.find("| 0x68 | MPU9250 accel/gyro") != std::string::npos);
  assert(output.find("no ACK") != std::string::npos);
  assert(output.find("| 0x42 | unbound device") != std::string::npos);
  assert(output.find("112/112 probed; 2 ACK; 1 unbound") != std::string::npos);

  for (const auto status : {HAL_BUSY, HAL_TIMEOUT}) {
    reset();
    failure = status;
    scan();
    assert(addresses.size() == 1);
    assert(output.find("not probed") != std::string::npos);
    assert(output.find("1/112 probed; 0 ACK") != std::string::npos);
    assert(output.find(status == HAL_BUSY ? "Scan result: bus busy"
                                          : "Scan result: probe timeout") !=
           std::string::npos);
  }
  reset();
  probe_cost = 100000; // Model HAL latency beyond its timeout argument.
  scan();
  assert(addresses.size() == 4);
  assert(output.find("4/112 probed") != std::string::npos);
  assert(output.find("Scan result: scan deadline") != std::string::npos);
  reset();
  all_ack = true;
  scan();
  assert(output.find("112/112 probed; 112 ACK; 110 unbound") !=
         std::string::npos);
  std::size_t rows = 0, offset = 0;
  while ((offset = output.find("unbound device", offset)) !=
         std::string::npos) {
    ++rows;
    ++offset;
  }
  assert(rows == 8);
  reset();
  bus.Instance = I2C2;
  expected_source = "i2c2";
  const i2c_diagnostics::Binding tof[] = {{0x29, "VL53L0X boot (shared)"}};
  i2c_diagnostics::scan(&bus, "ToF", "before init", tof, 1);
  assert(output.find("I2C2 / ToF / before init") != std::string::npos);
  assert(output.find("TCS34725") == std::string::npos);
  assert(output.find("VL53L0X boot") != std::string::npos);
  reset();
  expected_source = "i2c";
  i2c_diagnostics::scan(nullptr, "main", "unavailable", bindings, 2);
  assert(addresses.empty());
  assert(output.find("0/112 probed") != std::string::npos);

  reset();
  const i2c_diagnostics::Binding lsm[] = {{0x19, "LSM303DLHC accel"},
                                          {0x1e, "LSM303DLHC mag"}};
  i2c_diagnostics::scan_gpio(sda, scl, "main", lsm, 2);
  assert(addresses.empty()); // No STM32 HAL transaction before Wire.begin.
  assert(starts == 112 && stops == 112 && gpio_addresses.size() == 112);
  for (unsigned i = 0; i < 112; ++i)
    assert(gpio_addresses[i] == i + 8);
  assert(output.find("112/112 probed; 2 ACK; 0 unbound") != std::string::npos);
  assert(data_high && clock_high && modes[0] == INPUT && modes[1] == INPUT);

  reset();
  stuck_data = true;
  i2c_diagnostics::scan_gpio(sda, scl, "main", lsm, 2);
  assert(starts == 0 && clock_edges == 0); // Do not recover/mask stuck SDA.
  assert(output.find("Scan result: bus busy") != std::string::npos);
  assert(data_high && clock_high && modes[0] == INPUT && modes[1] == INPUT);

  reset();
  stretch = true;
  i2c_diagnostics::scan_gpio(sda, scl, "main", lsm, 2);
  assert(starts == 1 && clock_edges == 1 && gpio_addresses.empty());
  assert(output.find("Scan result: probe timeout") != std::string::npos);
  assert(now < 10000); // Bound clock stretching even without a HAL timeout.
  assert(data_high && clock_high && modes[0] == INPUT && modes[1] == INPUT);
}
