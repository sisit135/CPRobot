// SPDX-License-Identifier: GPL-3.0-only

#include "bup/bup_tasks.hpp"
#include "bup/i2c_isolation.hpp"
#include "openloop_demo/openloop_demo.hpp"
#if !AXI_OPENLOOP_DEMO
#include "hal/boot_sensor_sample.hpp"
#include "localization/hal_sensors.hpp"
#include "motor_control/hal_motor.hpp"
#include "remote/board_store.hpp"
#endif
#include "logging/log.hpp"
#include "platform/rtos.hpp"
#include <atomic>

namespace bup {
namespace {
std::atomic<BupStatus> bup_status{BupStatus::NOT_STARTED};
bool bup_started = false;
#if AXI_BUP_I2C_ISOLATION && !AXI_OPENLOOP_DEMO
void bup_task_entry(void *) {
  run_i2c_isolation();
  bup_status.store(BupStatus::FAILED, std::memory_order_release);
  vTaskDelete(nullptr);
}
#elif !AXI_OPENLOOP_DEMO
bool (*start_services)() = nullptr;
enum class Step : uint8_t { SKIPPED, READY, FAILED, DIAGNOSTIC };
struct Summary {
  boot_sensors::Snapshot sample;
  Step motor = Step::SKIPPED;
  Step settings = Step::SKIPPED;
  Step sensors = Step::SKIPPED;
  Step services = Step::SKIPPED;
};

bool initialize(Summary &summary) {
  robot_log::write(robot_log::Level::INFO, "bup", "bring-up started");
  // Inventory both buses before an unrelated HAL can alter pins or halt BUP.
  const bool buses_ready = hal_board_sensor_buses_init();
  summary.motor = Step::FAILED;
  if (!hal_motor_init()) {
    robot_log::write(robot_log::Level::ERROR, "drv",
                     "output init failed; halted");
    return false;
  }
  summary.motor = Step::READY;
  // DRV8833 provides no power-on feedback. This only confirms GPIO setup.
  robot_log::write(robot_log::Level::INFO, "drv",
                   "outputs configured; stopped");
  if (!buses_ready) {
    summary.sensors = Step::FAILED;
    robot_log::write(robot_log::Level::ERROR, "bup",
                     "sensor bus setup failed; halted");
    return false;
  }
  summary.settings = Step::FAILED;
  if (!remote::board_init()) {
    robot_log::write(robot_log::Level::ERROR, "remote",
                     "mandatory flash settings unavailable; halted");
    return false;
  }
  summary.settings = Step::READY;
  robot_log::write(robot_log::Level::INFO, "remote", "settings ready");
  summary.sensors = Step::FAILED;
  const bool sensors_ready = hal_sensors_init();
  boot_sensors::capture(summary.sample); // BUP still owns both sensor buses.
  if (!sensors_ready) {
    robot_log::write(robot_log::Level::ERROR, "bup",
                     "sensor init failed; halted");
    return false;
  }
#if AXI_BUP_IMU_DIAGNOSTIC
  // This bench mode never authorizes localization, navigation or motor tasks,
  // even if the IMU happens to pass on a later boot.
  summary.sensors = Step::DIAGNOSTIC;
  robot_log::write(robot_log::Level::ERROR, "bup",
                   "sensor diagnostics complete; services/localization halted");
  return false;
#endif
  summary.sensors = Step::READY;
  robot_log::write(robot_log::Level::INFO, "bup", "sensor HALs ready");
  if (start_services) {
    summary.services = Step::FAILED;
    if (!start_services()) {
      robot_log::write(robot_log::Level::ERROR, "bup",
                       "service startup failed; halted");
      return false;
    }
    summary.services = Step::READY;
  }
  return true;
}

void print_summary(const Summary &summary, bool success) {
  if (robot_log::minimum_level() < robot_log::Level::DEBUG)
    return;
  const auto result = [](Step step) {
    return step == Step::READY        ? "ready"
           : step == Step::FAILED     ? "failed"
           : step == Step::DIAGNOSTIC ? "diagnostic"
                                      : "skipped";
  };
  // UTF-8 box-drawing lines fit the fixed 96-byte log message buffer.
  robot_log::write(robot_log::Level::DEBUG, "bup",
                   "┌────────────┬────────────┐");
  robot_log::write(robot_log::Level::DEBUG, "bup",
                   "│ Step       │ Result     │");
  robot_log::write(robot_log::Level::DEBUG, "bup",
                   "├────────────┼────────────┤");
  robot_log::write(
      robot_log::Level::DEBUG, "bup", "│ %-10s │ %-10s │", "DRV8833",
      summary.motor == Step::READY ? "configured" : result(summary.motor));
  robot_log::write(robot_log::Level::DEBUG, "bup", "│ %-10s │ %-10s │",
                   "Settings", result(summary.settings));
  robot_log::write(robot_log::Level::DEBUG, "bup", "│ %-10s │ %-10s │",
                   "Sensors", result(summary.sensors));
  robot_log::write(robot_log::Level::DEBUG, "bup", "│ %-10s │ %-10s │",
                   "Services", result(summary.services));
  robot_log::write(robot_log::Level::DEBUG, "bup",
                   "├────────────┼────────────┤");
  robot_log::write(robot_log::Level::DEBUG, "bup", "│ %-10s │ %-10s │", "BUP",
                   success ? "ready" : "HALTED");
  robot_log::write(robot_log::Level::DEBUG, "bup",
                   "└────────────┴────────────┘");
}

void bup_task_entry(void *context) {
  (void)context;
  Summary summary;
  const bool success = initialize(summary);
  if (!success)
    hal_motor_stop();
  bup_status.store(success ? BupStatus::SUCCEEDED : BupStatus::FAILED,
                   std::memory_order_release);
  (void)robot_log::write(
      success ? robot_log::Level::INFO : robot_log::Level::ERROR, "bup",
      "bring-up %s", success ? "succeeded" : "failed");
  print_summary(summary, success);
  boot_sensors::print(summary.sample);
  vTaskDelete(nullptr);
}
#else
void bup_task_entry(void *) {
  robot_log::write(robot_log::Level::INFO, "bup",
                   "DEMO: inert; sensor/settings/services init skipped");
  bup_status.store(BupStatus::SUCCEEDED, std::memory_order_release);
  vTaskDelete(nullptr);
}
#endif
} // namespace

bool bup_task_start(bool (*services)()) {
  if (bup_started)
    return false;

  bup_started = true;
#if AXI_OPENLOOP_DEMO || AXI_BUP_I2C_ISOLATION
  (void)services;
#else
  start_services = services;
#endif

  robot_log::write(robot_log::Level::INFO, "bup", "Bringing up AXI THS platform STM32 MCU >> Hello from KMiTL RAI LabRoom");

  bup_status.store(BupStatus::RUNNING, std::memory_order_release);
  TaskHandle_t task = nullptr;
  if (xTaskCreate(bup_task_entry, "bup_task", 1024, nullptr, 1, &task) !=
      pdPASS) {
    bup_status.store(BupStatus::FAILED, std::memory_order_release);
    bup_started = false;
#if !AXI_OPENLOOP_DEMO && !AXI_BUP_I2C_ISOLATION
    hal_motor_stop();
#endif
    robot_log::write(robot_log::Level::ERROR, "bup",
#if AXI_OPENLOOP_DEMO
                     "task creation failed; demo remains independent");
#else
                     "task creation failed; halted");
#endif
    return false;
  }
  return true;
}

BupStatus bup_task_get_status() {
  return bup_status.load(std::memory_order_acquire);
}
} // namespace bup
