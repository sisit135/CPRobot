// SPDX-License-Identifier: GPL-3.0-only
#include "telemetry_codec.h"
#include "proto_buffer.hpp"
#include "telemetry.h"

size_t axi_ekf_encode(const axi_ekf_log *v, uint8_t *out, size_t capacity) {
  if (!v || !out)
    return 0;
  axi::telemetry::v1::EkfLog message;
  message.set_boot_id(v->boot_id);
  message.set_monotonic_us(v->monotonic_us);
  message.set_sequence(v->sequence);
  message.set_x_m(v->x_m);
  message.set_y_m(v->y_m);
  message.set_yaw_rad(v->yaw_rad);
  message.set_gyro_bias_rps(v->gyro_bias_rps);
  message.set_sigma_x_m(v->sigma_x_m);
  message.set_sigma_y_m(v->sigma_y_m);
  message.set_sigma_yaw_rad(v->sigma_yaw_rad);
  message.set_grid_x(v->grid_x);
  message.set_grid_y(v->grid_y);
  message.set_tracking(v->tracking);
  message.set_grid_confident(v->grid_confident);
  message.set_queue_dropped(v->queue_dropped);
  return tool_proto::encode(message, out, capacity);
}
size_t axi_log_encode(const axi_log_record *v, uint8_t *out, size_t capacity) {
  if (!v || !out || !v->source || !v->message)
    return 0;
  axi::telemetry::v1::LogRecord<64, 160> message;
  message.set_timestamp_ms(v->timestamp_ms);
  message.set_level(v->level);
  if (strlen(v->source) > 64 || strlen(v->message) > 160)
    return 0;
  message.mutable_source().set(v->source);
  message.mutable_message().set(v->message);
  message.set_collector_dropped(v->dropped);
  return tool_proto::encode(message, out, capacity);
}
