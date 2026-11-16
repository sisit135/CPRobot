// SPDX-License-Identifier: GPL-3.0-only

#include "telemetry/adapter.hpp"
#ifdef AXI_TELEMETRY_SPI_ENABLE
#include "../../../../../generic_field_tool/common/telemetry_codec.h"
#include "../../../../telemetry_bridge/common/wire.h"
#include "localization/localization.hpp"
#include "logging/log.hpp"
#include "motor_control/motor_control.hpp"
#include "navigation/navigation.hpp"
#include "platform/hal_platform.hpp"
#include "platform/rtos.hpp"
#include "remote/board_store.hpp"
#include "remote/schema_blob.hpp"
#include "remote/settings.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <cmath>
#include <cstring>
#include <semphr.h>
#ifndef AXI_SPI_CS_PIN
#define AXI_SPI_CS_PIN -1
#endif
#ifndef AXI_SPI_READY_PIN
#define AXI_SPI_READY_PIN -1
#endif
#ifndef AXI_LOC_KEY_PIN
#define AXI_LOC_KEY_PIN -1
#endif
namespace telemetry {
namespace {
QueueHandle_t snapshots = nullptr;
SemaphoreHandle_t start_mutex = nullptr;
nav::ResolverConfig settings{};
bool navigation_started = false;
bool stop_latched = false;
uint32_t sequence = 0;
uint32_t boot_id = 0;
uint8_t rx[AXI_SPI_SIZE], tx[AXI_SPI_SIZE];
uint32_t ack_id = 0;
uint8_t ack_op = 0, ack_status = 0;
uint32_t last_command_id = 0;
uint8_t remote_reply[238];
size_t remote_reply_size = 0;
bool calibrate_gyro(float *out) {
  // The sensor task owns I2C; only consume its fresh, published samples.
  float sum = 0, squares = 0;
  unsigned count = 0;
  int64_t last = 0;
  const int64_t deadline = hal_time_us() + 2000000;
  while (count < 64 && hal_time_us() < deadline) {
    nav::BootstrapFrame frame{};
    if (loc::localization_get_bootstrap(&frame) && frame.imu_us > last &&
        hal_time_us() >= frame.imu_us && hal_time_us() - frame.imu_us < 50000) {
      last = frame.imu_us;
      if (!std::isfinite(frame.imu.gz_rps) ||
          std::fabs(frame.imu.gx_rps) > .2f ||
          std::fabs(frame.imu.gy_rps) > .2f ||
          std::fabs(frame.imu.gz_rps) > .2f ||
          !std::isfinite(frame.imu.ax_mps2) ||
          !std::isfinite(frame.imu.ay_mps2) ||
          !std::isfinite(frame.imu.az_mps2) ||
          std::fabs(std::hypot(std::hypot(frame.imu.ax_mps2, frame.imu.ay_mps2),
                               frame.imu.az_mps2) -
                    9.80665f) > .3f)
        return false;
      sum += frame.imu.gz_rps;
      squares += frame.imu.gz_rps * frame.imu.gz_rps;
      ++count;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (count != 64 || squares / 64.0f - (sum / 64.0f) * (sum / 64.0f) > .0001f)
    return false;
  *out = sum / 64.0f + remote::gyro_zero();
  return true;
}
uint8_t calibrate_color(uint8_t color_id) {
  if (color_id >= 4)
    return 2;
  auto *store = remote::board_store();
  if (!store)
    return 1;
  nav::BootstrapFrame frame{};
  const int64_t now = hal_time_us();
  if (!loc::localization_get_bootstrap(&frame) || frame.color_us <= 0 ||
      now < frame.color_us || now - frame.color_us > 150000 ||
      !frame.color.sequence || frame.color.saturated || frame.color.clear < 100)
    return 1;
  const float red = frame.color.red, green = frame.color.green,
              blue = frame.color.blue;
  if (red + green + blue <= 0)
    return 1;
  const auto status = store->calibrate_color(color_id, red, green, blue,
                                             store->snapshot().revision);
  if (status != remote::OK)
    return 1;
  remote::apply(*store);
  return 0;
}
bool send_log(const robot_log::Record &record, void *) {
  axi_log_record wire_record{};
  wire_record.timestamp_ms = record.timestamp_ms;
  wire_record.level = static_cast<uint8_t>(record.level);
  wire_record.source = record.source;
  wire_record.message = record.message;
  wire_record.dropped = robot_log::dropped();
  uint8_t proto[AXI_MAX_TYPED_PROTO];
  const size_t len = axi_log_encode(&wire_record, proto, sizeof proto);
  if (!len)
    return false;
  return axi_spi_typed_pack(7, proto, len, tx) != 0;
}
bool begin_localization() {
  xSemaphoreTake(start_mutex, portMAX_DELAY);
  const bool ok =
      !stop_latched && remote::board_store() &&
      (navigation_started ||
       nav::navigation_start(settings, nav::NavigationEngine::FEATURE,
                             remote::feature_settings(),
                             remote::fallback_settings()));
  if (ok)
    navigation_started = true;
  xSemaphoreGive(start_mutex);
  return ok;
}
void stop_navigation() {
  xSemaphoreTake(start_mutex, portMAX_DELAY);
  stop_latched = true;
  nav::navigation_halt();
  motor::motor_stop();
  xSemaphoreGive(start_mutex);
}
/** @brief Sample the active-low GPIO key independently of the SPI transport.
 * @param[in] arg Unused task argument.
 * @returns Never returns.
 */
void key_task(void *arg) {
  (void)arg;
  pinMode(AXI_LOC_KEY_PIN, INPUT_PULLUP);
  unsigned low_samples = 0;
  bool pressed = false;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    if (digitalRead(AXI_LOC_KEY_PIN) == LOW) {
      if (low_samples < 3)
        ++low_samples;
      if (low_samples == 3 && !pressed) {
        pressed = true;
        (void)begin_localization();
      }
    } else {
      low_samples = 0;
      pressed = false;
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(20));
  }
}
void worker(void *) {
  SPI.begin();
  pinMode(AXI_SPI_CS_PIN, OUTPUT);
  digitalWrite(AXI_SPI_CS_PIN, HIGH);
  pinMode(AXI_SPI_READY_PIN, INPUT);
  boot_id = (uint32_t)hal_time_us() ^ (uint32_t)micros() ^ 0xAE194371u;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    memset(tx, 0, sizeof tx);
    // Logs are drained here, so neither a sensor driver nor a producer touches
    // SPI.
    if (remote_reply_size)
      (void)axi_spi_typed_pack(5, remote_reply, remote_reply_size, tx);
    else if (robot_log::drain(1) == 0) {
      loc::PoseSnapshot pose{};
      if (xQueueReceive(snapshots, &pose, 0) == pdTRUE) {
        axi_ekf_log log{};
        log.boot_id = boot_id;
        log.monotonic_us = (uint64_t)pose.timestamp_us;
        log.sequence = ++sequence;
        log.queue_dropped = pose.queue_dropped;
        log.x_m = pose.x_m;
        log.y_m = pose.y_m;
        log.yaw_rad = pose.yaw_rad;
        log.gyro_bias_rps = pose.gyro_bias_rps;
        log.sigma_x_m = pose.sigma_x_m;
        log.sigma_y_m = pose.sigma_y_m;
        log.sigma_yaw_rad = pose.sigma_yaw_rad;
        log.grid_x = pose.grid_x;
        log.grid_y = pose.grid_y;
        log.tracking = pose.tracking;
        log.grid_confident = pose.grid_confident;
        uint8_t proto[AXI_MAX_PROTO];
        const size_t len = axi_ekf_encode(&log, proto, sizeof proto);
        if (len)
          (void)axi_spi_pack(proto, len, tx);
      }
    }
    if (ack_id)
      axi_control_pack(tx + 246, AXI_SPI_ACK_MAGIC1, ack_id, ack_op,
                       ack_status);
    if (digitalRead(AXI_SPI_READY_PIN) == HIGH) {
      SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
      digitalWrite(AXI_SPI_CS_PIN, LOW);
      for (unsigned i = 0; i < AXI_SPI_SIZE; ++i)
        rx[i] = SPI.transfer(tx[i]);
      digitalWrite(AXI_SPI_CS_PIN, HIGH);
      SPI.endTransaction();
      uint32_t id = 0;
      uint8_t op = 0, status = 0;
      if (axi_control_unpack(rx, AXI_SPI_CMD_MAGIC1, &id, &op, &status) &&
          id != 0) {
        if (id != last_command_id) {
          ack_status = 1;
          if (op == 1) {
            if (begin_localization())
              ack_status = 0;
          } else if (op == 2) {
            stop_navigation();
            ack_status = 0;
          } else if (op == 3) {
            xSemaphoreTake(start_mutex, portMAX_DELAY);
            ack_status = (!navigation_started && !stop_latched)
                             ? calibrate_color(status)
                             : 1;
            xSemaphoreGive(start_mutex);
          } else {
            ack_status = 2;
          }
          last_command_id = id;
        }
        ack_id = id;
        ack_op = op;
      } else if (ack_id)
        ack_id = 0; // ESP has had one opportunity to see the ACK.
      remote_reply_size = 0;
      const uint8_t *proto = nullptr;
      size_t proto_size = 0;
      uint8_t kind = 0;
      if (axi_spi_typed_unpack(rx, &kind, &proto, &proto_size) && kind == 4) {
        remote_request request{};
        if (remote_decode(proto, proto_size, &request)) {
          xSemaphoreTake(start_mutex, portMAX_DELAY);
          auto *store = remote::board_store();
          remote_reply_size =
              store ? remote::handle(
                          *store, request, !navigation_started && !stop_latched,
                          remote::schema_blob, sizeof remote::schema_blob,
                          remote_reply, sizeof remote_reply, calibrate_gyro)
                    : remote_status(remote_reply, sizeof remote_reply,
                                    request.id, remote::NOT_READY);
          xSemaphoreGive(start_mutex);
        }
      }
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(50));
  }
}
} // namespace
bool start(const nav::ResolverConfig &config) {
  if (snapshots || AXI_SPI_CS_PIN < 0 || AXI_SPI_READY_PIN < 0)
    return false;
  settings = config;
  snapshots = xQueueCreate(1, sizeof(loc::PoseSnapshot));
  start_mutex = xSemaphoreCreateMutex();
  if (!snapshots || !start_mutex) {
    if (snapshots)
      vQueueDelete(snapshots);
    if (start_mutex)
      vSemaphoreDelete(start_mutex);
    snapshots = nullptr;
    start_mutex = nullptr;
    return false;
  }
  TaskHandle_t spi_handle = nullptr, key_handle = nullptr;
  if (xTaskCreate(worker, "ekf_spi", 2048, nullptr, 1, &spi_handle) != pdPASS ||
      (AXI_LOC_KEY_PIN >= 0 && xTaskCreate(key_task, "loc_key", 512, nullptr, 2,
                                           &key_handle) != pdPASS)) {
    if (spi_handle)
      vTaskDelete(spi_handle);
    if (key_handle)
      vTaskDelete(key_handle);
    vSemaphoreDelete(start_mutex);
    start_mutex = nullptr;
    vQueueDelete(snapshots);
    snapshots = nullptr;
    return false;
  }
  (void)robot_log::set_sink(robot_log::TELEMETRY, send_log);
  return true;
}
void submit(const loc::PoseSnapshot &pose) {
  if (snapshots)
    xQueueOverwrite(snapshots, &pose);
}
} // namespace telemetry
#endif
