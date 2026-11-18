// SPDX-License-Identifier: GPL-3.0-only
#include "hal/i2c_diagnostics.hpp"
#include "logging/log.hpp"
#include "platform/hal_platform.hpp"
#include "platform/rtos.hpp"
#include <Arduino.h>

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
template <typename Probe>
void inventory(Controller physical, const char *name, const char *phase,
               const Binding *bindings, std::size_t count, bool available,
               Probe probe) {
  const char *source = physical.source;
  Result results[128]{};
  unsigned probed = 0, found = 0, unknown = 0;
  const char *completion = "bus unavailable";
  const int64_t started = hal_time_us();
  if (available) {
    completion = "complete";
    // Exclude reserved 7-bit addresses. HAL expects the left-shifted address.
    for (unsigned address = 0x08; address <= 0x77; ++address) {
      if (hal_time_us() - started >= 400000) {
        completion = "scan deadline";
        break;
      }
      results[address] = probe(address);
      ++probed;
      if (results[address] == ACK)
        ++found;
      vTaskDelay(1);
      if (results[address] == BUS_BUSY || results[address] == TIMEOUT) {
        completion =
            results[address] == BUS_BUSY ? "bus busy" : "probe timeout";
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

struct GpioMaster {
  int sda, scl;
  void set(int pin, bool high) {
    digitalWrite(pin, high ? HIGH : LOW); // Open-drain: HIGH only releases.
    delayMicroseconds(5);
  }
  bool clock_high() {
    set(scl, true);
    const auto start = hal_time_us();
    while (!digitalRead(scl)) {
      if (hal_time_us() - start >= 1000)
        return false;
      delayMicroseconds(5);
    }
    return true;
  }
  void release() {
    digitalWrite(sda, HIGH);
    digitalWrite(scl, HIGH);
  }
  Result probe(unsigned address) {
    if (!digitalRead(scl) || !digitalRead(sda))
      return BUS_BUSY; // Preserve a stuck bus; no recovery pulses.
    set(sda, false);   // START while SCL high.
    set(scl, false);
    const auto timeout = [&] {
      release();
      return TIMEOUT;
    };
    for (uint8_t mask = 0x80, byte = uint8_t(address << 1); mask; mask >>= 1) {
      set(sda, byte & mask);
      if (!clock_high())
        return timeout();
      set(scl, false);
    }
    set(sda, true); // Slave owns ninth bit.
    if (!clock_high())
      return timeout();
    const bool ack = !digitalRead(sda);
    set(scl, false);
    set(sda, false);
    if (!clock_high())
      return timeout();
    set(sda, true); // STOP; no payload byte or register pointer.
    return ack ? ACK : NO_ACK;
  }
};
} // namespace

void scan(I2C_HandleTypeDef *bus, const char *name, const char *phase,
          const Binding *bindings, std::size_t count) {
  inventory(controller(bus), name, phase, bindings, count, bus && bus->Instance,
            [&](unsigned address) {
              const auto status =
                  HAL_I2C_IsDeviceReady(bus, address << 1, 1, 2);
              return status == HAL_OK        ? ACK
                     : status == HAL_BUSY    ? BUS_BUSY
                     : status == HAL_TIMEOUT ? TIMEOUT
                                             : NO_ACK;
            });
}

void scan_gpio(int sda, int scl, const char *name, const Binding *bindings,
               std::size_t count) {
  // Enable GPIO clocks with both drivers released before preloading ODR.
  // A write to an unclocked port before pinMode() need not latch HIGH.
  pinMode(sda, INPUT);
  pinMode(scl, INPUT);
  robot_log::write(robot_log::Level::INFO, "i2c",
                   "%s GPIO before Wire.begin: SDA=%d SCL=%d", name,
                   digitalRead(sda), digitalRead(scl));
  GpioMaster gpio{sda, scl};
  gpio.release(); // Preload release before enabling the output drivers.
  pinMode(sda, OUTPUT_OPEN_DRAIN);
  pinMode(scl, OUTPUT_OPEN_DRAIN);
  delayMicroseconds(5);
  inventory({"GPIO", "i2c"}, name, "before Wire.begin", bindings, count, true,
            [&](unsigned address) { return gpio.probe(address); });
  gpio.release();
  pinMode(sda, INPUT);
  pinMode(scl, INPUT);
}

void state(I2C_HandleTypeDef *bus, int sda, int scl, const char *phase) {
  const auto physical = controller(bus);
  robot_log::write(robot_log::Level::INFO, physical.source, "%s: SDA=%d SCL=%d",
                   phase, digitalRead(sda), digitalRead(scl));
#if defined(STM32F411xE)
  if (bus && bus->Instance) {
    // Read SR2 before SR1: the opposite order can clear ADDR in hardware.
    const uint32_t sr2 = bus->Instance->SR2;
    const uint32_t sr1 = bus->Instance->SR1;
    robot_log::write(robot_log::Level::INFO, physical.source,
                     "CR1=%04lx CR2=%04lx SR1=%04lx SR2=%04lx",
                     bus->Instance->CR1, bus->Instance->CR2, sr1, sr2);
    robot_log::write(robot_log::Level::INFO, physical.source,
                     "state=%02x error=%08lx CCR=%04lx TRISE=%02lx",
                     unsigned(bus->State), bus->ErrorCode, bus->Instance->CCR,
                     bus->Instance->TRISE);
  }
#endif
}
} // namespace i2c_diagnostics
