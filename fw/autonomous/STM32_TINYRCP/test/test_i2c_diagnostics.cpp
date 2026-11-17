// SPDX-License-Identifier: GPL-3.0-only
#include "hal/i2c_diagnostics.hpp"
#include "logging/log.hpp"
#include "platform/rtos.hpp"
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
std::string expected_source = "i2c1";
void reset() {
  now = 1;
  probe_cost = 100;
  failure = HAL_ERROR;
  all_ack = false;
  addresses.clear();
  output.clear();
}
} // namespace
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
}
