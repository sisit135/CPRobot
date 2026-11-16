// SPDX-License-Identifier: GPL-3.0-only

#include "localization/localization.hpp"
#include "logging/log.hpp"
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
const char *resolver_name(ResolveStatus state) {
  switch (state) {
  case ResolveStatus::WAIT_CALIBRATION:
    return "WAIT_CALIBRATION";
  case ResolveStatus::WAIT_SENSORS:
    return "WAIT_SENSORS";
  case ResolveStatus::NO_CANDIDATE:
    return "NO_CANDIDATE";
  case ResolveStatus::AMBIGUOUS:
    return "AMBIGUOUS";
  case ResolveStatus::CONFIRMING:
    return "CONFIRMING";
  case ResolveStatus::RESOLVED:
    return "RESOLVED";
  case ResolveStatus::INVALID_CONFIG:
    return "INVALID_CONFIG";
  }
  return "UNKNOWN";
}
struct ResolverLogState {
  Resolution resolution{};
  std::array<uint32_t, 4> cells{};
  unsigned candidates = 0;
  uint8_t excluded_rays = 0;
  int64_t state_since_us = 0, last_wait_report_us = 0;
  bool reported = false, tracking = false;
  void update(const FeatureSnapshot &s, int64_t now) {
    const auto &r = s.resolution;
    const bool state_changed = !reported || r.status != resolution.status;
    const bool set_changed = s.candidate_cells != cells;
    const bool tracking_changed = !reported || s.pose.tracking != tracking;
    if (s.excluded_ray_mask != excluded_rays) {
      robot_log::write(robot_log::Level::INFO, "resolver",
                       "range associations excluded L/F/R mask=0x%x",
                       unsigned(s.excluded_ray_mask));
      excluded_rays = s.excluded_ray_mask;
    }
    if (state_changed)
      state_since_us = now;
    if (state_changed || set_changed || s.candidates != candidates ||
        tracking_changed)
      robot_log::write(robot_log::Level::INFO, "resolver",
                       "state=%s candidates=%u tracking=%u fit_us=%lld",
                       resolver_name(r.status), s.candidates,
                       unsigned(s.pose.tracking),
                       static_cast<long long>(s.candidate_epoch_us));
    if (r.status != ResolveStatus::RESOLVED &&
        now - last_wait_report_us >= 2000000) {
      robot_log::write(
          robot_log::Level::INFO, "resolver",
          "waiting=%s state_ms=%lld candidates=%u fit_age_ms=%lld",
          resolver_name(r.status),
          static_cast<long long>((now - state_since_us) / 1000), s.candidates,
          static_cast<long long>(s.candidate_epoch_us > 0
                                     ? (now - s.candidate_epoch_us) / 1000
                                     : -1));
      last_wait_report_us = now;
    }
    unsigned changes = 0;
    for (unsigned v = 0; v < cells.size(); ++v)
      for (uint32_t bits = cells[v] ^ s.candidate_cells[v]; bits;
           bits &= bits - 1)
        ++changes;
    // At most ten records per update, including state and selection. Large
    // candidate bursts use exact masks instead of overflowing the log ring.
    for (unsigned v = 0; v < cells.size(); ++v) {
      const uint32_t changed = cells[v] ^ s.candidate_cells[v];
      if (changes > 8) {
        if (changed)
          robot_log::write(
              robot_log::Level::INFO, "resolver",
              "candidate set Q%u add=0x%07lx remove=0x%07lx", v + 1,
              static_cast<unsigned long>(s.candidate_cells[v] & ~cells[v]),
              static_cast<unsigned long>(cells[v] & ~s.candidate_cells[v]));
        continue;
      }
      for (unsigned bit = 0; bit < 25; ++bit)
        if (changed & (uint32_t{1} << bit))
          robot_log::write(
              robot_log::Level::INFO, "resolver",
              "candidate %s Q%u cell=(%u,%u)",
              s.candidate_cells[v] & (uint32_t{1} << bit) ? "add" : "remove",
              v + 1, bit % 5, bit / 5);
    }
    if ((r.status == ResolveStatus::CONFIRMING ||
         r.status == ResolveStatus::RESOLVED) &&
        (state_changed || tracking_changed || r.kind != resolution.kind ||
         r.cell != resolution.cell ||
         (r.status == ResolveStatus::CONFIRMING &&
          r.confirmations != resolution.confirmations)))
      robot_log::write(
          robot_log::Level::INFO, "resolver",
          "update Q%u cell=(%d,%d) confirmations=%u score=%.2f margin=%.2f",
          unsigned(r.kind) + 1, r.cell.x, r.cell.y, r.confirmations,
          double(r.score), double(r.margin));
    resolution = r;
    cells = s.candidate_cells;
    candidates = s.candidates;
    tracking = s.pose.tracking;
    reported = true;
  }
};
void feature_task(void *) {
  // Fixed hypothesis buffers live in static storage, not the RTOS stack.
  static FeatureNavigation engine(maps, settings);
  ResolverLogState reported;
  unsigned pose_updates = 0;
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
    else
      reported.update(result, hal_time_us());
    // FEATURE publishes every 50 ms. Sample one pose record per ten updates
    // so detailed navigation readings remain available without flooding the
    // deferred log ring.
    if (++pose_updates == 10) {
      robot_log::write(
          robot_log::Level::TRACE, "pose",
          "x=%.3f y=%.3f yaw=%.3f track=%u cell=%d,%d sigma=%.3f,%.3f,%.3f",
          double(result.pose.x_m), double(result.pose.y_m),
          double(result.pose.yaw_rad), unsigned(result.pose.tracking),
          result.pose.grid_x, result.pose.grid_y, double(result.pose.sigma_x_m),
          double(result.pose.sigma_y_m), double(result.pose.sigma_yaw_rad));
      pose_updates = 0;
    }
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
