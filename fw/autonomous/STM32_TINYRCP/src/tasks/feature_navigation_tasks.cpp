#include "localization/localization.hpp"
#include "motor_control/motor_control.hpp"
#include "navigation/feature_tasks.hpp"
#include "platform/hal_platform.hpp"
#include "platform/rtos.hpp"
#include <algorithm>
#include <atomic>
namespace nav {
namespace {
QueueHandle_t snapshots = nullptr, targets = nullptr;
ResolverConfig maps;
FeatureConfig settings;
std::atomic<bool> stopped{false};
void feature_task(void *) {
  // Fixed hypothesis buffers live in static storage, not the RTOS stack.
  static FeatureNavigation engine(maps, settings);
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    if (stopped.load(std::memory_order_acquire) ||
        motor::motor_run_state().terminal()) {
      FeatureSnapshot invalid{};
      xQueueOverwrite(snapshots, &invalid);
      vTaskDelete(nullptr);
      return;
    }
    loc::Cell target{};
    if (xQueueReceive(targets, &target, 0) == pdTRUE)
      engine.set_stop_target(target);
    BootstrapFrame frame{};
    loc::localization_get_bootstrap(&frame);
    auto result = engine.update(frame, hal_time_us());
    // A timeout may have occurred during the bounded map search.
    if (motor::motor_run_state().terminal())
      result = {};
    xQueueOverwrite(snapshots, &result);
    vTaskDelayUntil(&wake, std::max<TickType_t>(1, pdMS_TO_TICKS(50)));
  }
}
} // namespace
bool feature_task_start(const ResolverConfig &c, const FeatureConfig &f) {
  if (snapshots || targets || stopped.load(std::memory_order_acquire))
    return false;
  maps = c;
  settings = f;
  snapshots = xQueueCreate(1, sizeof(FeatureSnapshot));
  targets = xQueueCreate(1, sizeof(loc::Cell));
  TaskHandle_t task = nullptr;
  if (!snapshots || !targets ||
      xTaskCreate(feature_task, "feature_nav", 2048, nullptr, 1, &task) !=
          pdPASS) {
    if (snapshots)
      vQueueDelete(snapshots);
    if (targets)
      vQueueDelete(targets);
    snapshots = targets = nullptr;
    return false;
  }
  return true;
}
void feature_task_halt() { stopped.store(true, std::memory_order_release); }
bool feature_task_snapshot(FeatureSnapshot *s) {
  if (!s || !snapshots || xQueuePeek(snapshots, s, 0) != pdTRUE)
    return false;
  const auto now = hal_time_us();
  if (stopped.load(std::memory_order_acquire) ||
      motor::motor_run_state().terminal() || s->pose.timestamp_us <= 0 ||
      s->pose.timestamp_us > now || now - s->pose.timestamp_us > 150000) {
    s->pose.tracking = false;
    s->stop.valid = false;
    if (stopped.load(std::memory_order_acquire) ||
        motor::motor_run_state().terminal())
      s->heading.valid = false;
  }
  return true;
}
void feature_task_target(loc::Cell c) {
  if (targets)
    xQueueOverwrite(targets, &c);
}
} // namespace nav
