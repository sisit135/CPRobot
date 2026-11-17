// SPDX-License-Identifier: GPL-3.0-only
#include "hal/i2c_diagnostics.hpp"
#include "logging/log.hpp"
#include "platform/hal_platform.hpp"
#include "platform/rtos.hpp"

namespace i2c_diagnostics {
namespace {
struct Controller {
  const char *name;
  const char *source;
};
Controller controller(I2C_HandleTypeDef *bus) {
  if (bus) {
#ifdef I2C1
    if (bus->Instance == I2C1)
      return {"I2C1", "i2c1"};
#endif
#ifdef I2C2
    if (bus->Instance == I2C2)
      return {"I2C2", "i2c2"};
#endif
#ifdef I2C3
    if (bus->Instance == I2C3)
      return {"I2C3", "i2c3"};
#endif
  }
  return {"unknown controller", "i2c"};
}
enum Result : uint8_t { NOT_PROBED, ACK, NO_ACK, TIMEOUT, BUS_BUSY };
const char *label(Result result) {
  switch (result) {
  case ACK:
    return "ACK";
  case NO_ACK:
    return "no ACK";
  case TIMEOUT:
    return "timeout";
  case BUS_BUSY:
    return "bus busy";
  default:
    return "not probed";
  }
}
void row(const char *source, uint8_t address, const char *device,
         Result result) {
  robot_log::write(robot_log::Level::INFO, source, "| 0x%02x | %-25s | %-10s |",
                   unsigned(address), device, label(result));
  // Let boot_log drain the table even when many devices acknowledge.
  vTaskDelay(1);
}
} // namespace

void scan(I2C_HandleTypeDef *bus, const char *name, const char *phase,
          const Binding *bindings, std::size_t count) {
  const auto physical = controller(bus);
  const char *source = physical.source;
  Result results[128]{};
  unsigned probed = 0, found = 0, unknown = 0;
  const char *completion = "bus unavailable";
  const int64_t started = hal_time_us();
  if (bus && bus->Instance) {
    completion = "complete";
    // Exclude reserved 7-bit addresses. HAL expects the left-shifted address.
    for (unsigned address = 0x08; address <= 0x77; ++address) {
      if (hal_time_us() - started >= 400000) {
        completion = "scan deadline";
        break;
      }
      const auto status = HAL_I2C_IsDeviceReady(bus, address << 1, 1, 2);
      results[address] = status == HAL_OK        ? ACK
                         : status == HAL_TIMEOUT ? TIMEOUT
                         : status == HAL_BUSY    ? BUS_BUSY
                                                 : NO_ACK;
      ++probed;
      if (status == HAL_OK)
        ++found;
      vTaskDelay(1);
      if (status == HAL_BUSY || status == HAL_TIMEOUT) {
        completion = status == HAL_BUSY ? "bus busy" : "probe timeout";
        break; // A stalled controller must not delay BUP per remaining address.
      }
    }
  }
  robot_log::write(robot_log::Level::INFO, source, "%s / %s / %s: address scan",
                   physical.name, name, phase);
  robot_log::write(robot_log::Level::INFO, source,
                   "+------+---------------------------+------------+");
  robot_log::write(robot_log::Level::INFO, source,
                   "| Addr | Configured binding        | Probe      |");
  for (std::size_t i = 0; i < count; ++i)
    row(source, bindings[i].address, bindings[i].device,
        bindings[i].address < 128 ? results[bindings[i].address] : NOT_PROBED);
  for (unsigned address = 0x08; address <= 0x77; ++address) {
    if (results[address] != ACK)
      continue;
    bool bound = false;
    for (std::size_t i = 0; i < count; ++i)
      bound = bound || bindings[i].address == address;
    if (!bound && unknown++ < 8)
      row(source, address, "unbound device", ACK);
  }
  robot_log::write(robot_log::Level::INFO, source,
                   "+------+---------------------------+------------+");
  robot_log::write(robot_log::Level::INFO, source,
                   "%u/112 probed; %u ACK; %u unbound (shown at most 8)",
                   probed, found, unknown);
  robot_log::write(robot_log::Level::INFO, source, "Scan result: %s",
                   completion);
  robot_log::write(
      robot_log::Level::INFO, source,
      "ACK confirms address response only; identity checked by HAL");
}
} // namespace i2c_diagnostics
