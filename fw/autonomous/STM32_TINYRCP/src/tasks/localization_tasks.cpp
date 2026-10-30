#include "localization/config.hpp"
#include "localization/fusion_engine.hpp"
#include "localization/hal_sensors.hpp"
#include "localization/localization.hpp"
#include "platform/hal_platform.hpp"
#include "platform/rtos.hpp"
#include <algorithm>
#include <atomic>
#include <cinttypes>

namespace loc {
namespace {
constexpr EventBits_t kStart = 1, kFusionStart = 2;
QueueHandle_t s_events = nullptr, s_pose = nullptr, s_bootstrap = nullptr;
MazeMap s_map;
nav::Resolution s_seed;
std::atomic<bool> s_fusing{false}, s_ready{false}, s_halted{false},
    s_fusion_stopped{false};
EventGroupHandle_t s_gate = nullptr;
std::atomic<uint32_t> s_dropped{0};
bool s_started = false, s_sensors_started = false;
TaskHandle_t s_sensor = nullptr, s_fusion = nullptr, s_logger = nullptr;

/** @brief Convert milliseconds to at least one RTOS tick.
 * @param[in] ms Requested interval in milliseconds.
 * @returns A nonzero interval in ticks.
 */
TickType_t ticks(uint32_t ms) {
  return std::max<TickType_t>(1, pdMS_TO_TICKS(ms));
}
/** @brief Wait for successful task startup.
 * @returns No value; returns only after the startup barrier opens.
 */
void wait_for_start() {
  xEventGroupWaitBits(s_gate, kStart, pdFALSE, pdTRUE, portMAX_DELAY);
}

/** @brief Poll all HAL devices from one task to serialize bus access.
 * @param[in] context Unused FreeRTOS task argument.
 * @returns Never returns.
 */
void sensor_task(void *context) {
  (void)context;
  wait_for_start();
  nav::BootstrapFrame frame{};
  TickType_t wake = xTaskGetTickCount();
  int64_t next_tof = 0, next_marker = 0;
  for (;;) {
    if (s_halted.load(std::memory_order_acquire)) {
      vTaskDelayUntil(&wake, ticks(cfg::sensor_period_ms));
      continue;
    }
    SensorEvent batch[6]{};
    int count = 0;
    SensorEvent e{};
    if (hal_imu_read(&e.imu, &e.timestamp_us)) {
      e.kind = SensorKind::IMU;
      if (e.timestamp_us == 0)
        e.timestamp_us = hal_time_us();
      frame.imu = e.imu;
      frame.imu_us = e.timestamp_us;
      batch[count++] = e;
    }
    e = {};
    if (hal_mag_read(&e.mag, &e.timestamp_us)) {
      e.kind = SensorKind::MAG;
      e.magnetic_clean = hal_magnetic_clean();
      if (e.timestamp_us == 0)
        e.timestamp_us = hal_time_us();
      frame.mag = e.mag;
      frame.mag_us = e.timestamp_us;
      frame.magnetic_clean = e.magnetic_clean;
      batch[count++] = e;
    }
    const int64_t now = hal_time_us();
    if (now >= next_tof) {
      next_tof = now + cfg::tof_period_us;
      for (unsigned i = 0; i < 3; ++i) {
        e = {};
        e.kind = SensorKind::TOF;
        e.tof_id = static_cast<TofId>(i);
        if (hal_tof_reddistancecorrected(e.tof_id, &e.range_m,
                                         &e.timestamp_us)) {
          if (e.timestamp_us == 0)
            e.timestamp_us = hal_time_us();
          frame.range_m[i] = e.range_m;
          frame.tof_us[i] = e.timestamp_us;
          batch[count++] = e;
        }
      }
    }
    if (now >= next_marker) {
      next_marker = now + cfg::marker_period_us;
      e = {};
      e.kind = SensorKind::MARKER;
      if (hal_tcs34725_read(&frame.color, &frame.color_us)) {
        // Black tape is an end-entry cue, never a color-based EKF fix.
        hal_end_tape_classify(frame.color, frame.color_us, &frame.end_tape);
      }
    }
    // Small stable insertion sort; no heap. True acquisition timestamps
    // can differ across sensors. Older than an earlier batch is rejected.
    for (int i = 1; i < count; ++i) {
      const SensorEvent value = batch[i];
      int j = i;
      while (j > 0 && batch[j - 1].timestamp_us > value.timestamp_us) {
        batch[j] = batch[j - 1];
        --j;
      }
      batch[j] = value;
    }
    xQueueOverwrite(s_bootstrap, &frame);
    for (int i = 0; s_fusing.load(std::memory_order_acquire) && i < count;
         ++i) {
      if (xQueueSend(s_events, &batch[i], 0) != pdTRUE)
        s_dropped.fetch_add(1, std::memory_order_relaxed);
    }
    vTaskDelayUntil(&wake, ticks(cfg::sensor_period_ms));
  }
}

/** @brief Own EKF state, process FIFO events, and publish immutable snapshots.
 * @param[in] context Unused FreeRTOS task argument.
 * @returns Never returns.
 */
void fusion_task(void *context) {
  (void)context;
  xEventGroupWaitBits(s_gate, kFusionStart, pdFALSE, pdTRUE, portMAX_DELAY);
  FusionEngine engine;
  if (!engine.initialize(s_map, s_seed.x_m, s_seed.y_m, s_seed.yaw_rad,
                         s_seed.heading_us)) {
    PoseSnapshot failed{};
    failed.health = FusionHealth::FAILED;
    failed.timestamp_us = hal_time_us();
    xQueueOverwrite(s_pose, &failed);
    vTaskDelete(nullptr);
    return;
  }
  if (s_halted.load(std::memory_order_acquire) ||
      s_fusion_stopped.load(std::memory_order_acquire)) {
    vTaskDelete(nullptr);
    return;
  }
  auto initial = engine.snapshot(hal_time_us());
  xQueueOverwrite(s_pose, &initial);
  s_ready.store(true, std::memory_order_release);
  s_fusing.store(true, std::memory_order_release);
  int64_t next_publish = 0;
  for (;;) {
    if (s_halted.load(std::memory_order_acquire) ||
        s_fusion_stopped.load(std::memory_order_acquire)) {
      vTaskDelay(ticks(cfg::publish_period_ms));
      continue;
    }
    SensorEvent e{};
    if (xQueueReceive(s_events, &e, ticks(cfg::publish_period_ms)) == pdTRUE &&
        !s_halted.load(std::memory_order_acquire) &&
        !s_fusion_stopped.load(std::memory_order_acquire))
      engine.process(e, hal_time_us());
    const int64_t now = hal_time_us();
    if (now >= next_publish && !s_halted.load(std::memory_order_acquire) &&
        !s_fusion_stopped.load(std::memory_order_acquire)) {
      PoseSnapshot p = engine.snapshot(now);
      p.queue_dropped = s_dropped.load(std::memory_order_relaxed);
      // Overwrite is used ONLY on this queue of length one.
      xQueueOverwrite(s_pose, &p);
      next_publish = now + static_cast<int64_t>(cfg::publish_period_ms) * 1000;
    }
  }
}

/** @brief Log the latest pose at a low rate without blocking the EKF.
 * @param[in] context Unused FreeRTOS task argument.
 * @returns Never returns.
 */
void logger_task(void *context) {
  (void)context;
  xEventGroupWaitBits(s_gate, kFusionStart, pdFALSE, pdTRUE, portMAX_DELAY);
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    PoseSnapshot p{};
    if (!s_halted.load(std::memory_order_acquire) &&
        !s_fusion_stopped.load(std::memory_order_acquire) &&
        localization_get_pose(&p)) {
      hal_log_pose(p);
    }
    vTaskDelayUntil(&wake, ticks(cfg::logger_period_ms));
  }
}

/** @brief Delete tasks still blocked at the startup barrier and their queues.
 * @returns No value.
 * @note Only called on startup failure, before kStart is set.
 */
void cleanup_startup() {
  for (TaskHandle_t *task : {&s_sensor, &s_fusion, &s_logger}) {
    if (*task) {
      vTaskDelete(*task);
      *task = nullptr;
    }
  }
  if (s_events) {
    vQueueDelete(s_events);
    s_events = nullptr;
  }
  if (s_pose) {
    vQueueDelete(s_pose);
    s_pose = nullptr;
  }
  if (s_gate) {
    vEventGroupDelete(s_gate);
    s_gate = nullptr;
  }
}
} // namespace

bool localization_sensors_start() {
  if (s_sensors_started || s_halted.load(std::memory_order_acquire))
    return false;
  s_events = xQueueCreate(cfg::event_queue_length, sizeof(SensorEvent));
  s_pose = xQueueCreate(1, sizeof(PoseSnapshot));
  s_bootstrap = xQueueCreate(1, sizeof(nav::BootstrapFrame));
  s_gate = xEventGroupCreate();
  if (!s_events || !s_pose || !s_bootstrap || !s_gate ||
      xTaskCreate(sensor_task, "loc_sensor", cfg::sensor_stack_words, nullptr,
                  cfg::sensor_priority, &s_sensor) != pdPASS) {
    cleanup_startup();
    if (s_bootstrap) {
      vQueueDelete(s_bootstrap);
      s_bootstrap = nullptr;
    }
    return false;
  }
  s_sensors_started = true;
  xEventGroupSetBits(s_gate, kStart);
  return true;
}
bool localization_start(const MazeMap &map, const nav::Resolution &seed) {
  if (s_started || s_halted.load(std::memory_order_acquire) ||
      s_fusion_stopped.load(std::memory_order_acquire) || !s_sensors_started ||
      !map.valid() || seed.status != nav::ResolveStatus::RESOLVED ||
      seed.kind != map.kind || seed.heading_us <= 0)
    return false;
  s_map = map;
  s_seed = seed;
  if (xTaskCreate(fusion_task, "loc_fusion", cfg::fusion_stack_words, nullptr,
                  cfg::fusion_priority, &s_fusion) != pdPASS ||
      xTaskCreate(logger_task, "loc_logger", cfg::logger_stack_words, nullptr,
                  cfg::logger_priority, &s_logger) != pdPASS) {
    if (s_fusion) {
      vTaskDelete(s_fusion);
      s_fusion = nullptr;
    }
    if (s_logger) {
      vTaskDelete(s_logger);
      s_logger = nullptr;
    }
    return false;
  }
  s_started = true;
  xEventGroupSetBits(s_gate, kFusionStart);
  return true;
}
void localization_stop_fusion() {
  s_fusion_stopped.store(true, std::memory_order_release);
  s_fusing.store(false, std::memory_order_release);
  s_ready.store(false, std::memory_order_release);
}
void localization_halt() {
  s_halted.store(true, std::memory_order_release);
  localization_stop_fusion();
}
bool localization_ready() { return s_ready.load(std::memory_order_acquire); }
bool localization_get_bootstrap(nav::BootstrapFrame *out) {
  return out && s_bootstrap && xQueuePeek(s_bootstrap, out, 0) == pdTRUE;
}

bool localization_get_pose(PoseSnapshot *out) {
  if (!out || !s_pose || xQueuePeek(s_pose, out, 0) != pdTRUE)
    return false;
  const int64_t now = hal_time_us();
  if (out->timestamp_us <= 0 ||
      now - out->timestamp_us > cfg::max_event_age_us ||
      out->last_imu_us <= 0 ||
      now - out->last_imu_us > cfg::tracking_imu_age_us ||
      now - out->last_x_fix_us > cfg::tracking_fix_age_us ||
      now - out->last_y_fix_us > cfg::tracking_fix_age_us) {
    out->tracking = false;
    out->grid_confident = false;
  }
  return true;
}
} // namespace loc
