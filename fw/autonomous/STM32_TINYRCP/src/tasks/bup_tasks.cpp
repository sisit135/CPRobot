#include "bup/bup_tasks.hpp"
#include "logging/log.hpp"
#include "platform/rtos.hpp"
#include <atomic>

namespace bup {
namespace {
std::atomic<BupStatus> bup_status{BupStatus::NOT_STARTED};
bool bup_started = false;

bool bup_power_rail_management() { return true; }

bool bup_sensor_test() { return true; }

void bup_task_entry(void *context) {
  (void)context;
  const bool success = bup_power_rail_management() && bup_sensor_test();
  bup_status.store(success ? BupStatus::SUCCEEDED : BupStatus::FAILED,
                   std::memory_order_release);
  (void)robot_log::write(
      success ? robot_log::Level::INFO : robot_log::Level::ERROR, "bup",
      "bring-up %s", success ? "succeeded" : "failed");
  vTaskDelete(nullptr);
}
} // namespace

bool bup_task_start() {
  if (bup_started)
    return false;

  bup_started = true;
  bup_status.store(BupStatus::RUNNING, std::memory_order_release);
  TaskHandle_t task = nullptr;
  if (xTaskCreate(bup_task_entry, "bup_task", 512, nullptr, 1, &task) !=
      pdPASS) {
    bup_status.store(BupStatus::FAILED, std::memory_order_release);
    bup_started = false;
    return false;
  }
  return true;
}

BupStatus bup_task_get_status() {
  return bup_status.load(std::memory_order_acquire);
}
} // namespace bup
