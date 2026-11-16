// SPDX-License-Identifier: GPL-3.0-only

#ifndef AXI_TELEMETRY_CODEC_H
#define AXI_TELEMETRY_CODEC_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* C ABI around bounded EmbeddedProto generated messages. */
typedef struct {
  uint32_t boot_id, sequence, queue_dropped;
  uint64_t monotonic_us;
  float x_m, y_m, yaw_rad, gyro_bias_rps, sigma_x_m, sigma_y_m, sigma_yaw_rad;
  int32_t grid_x, grid_y;
  uint8_t tracking, grid_confident;
} axi_ekf_log;
typedef struct {
  uint32_t timestamp_ms, dropped;
  uint8_t level;
  const char *source, *message;
} axi_log_record;
/** @brief Encode one EkfLog using EmbeddedProto.
 * @param[in] log Values to serialize.
 * @param[out] out Output byte array.
 * @param[in] capacity Available bytes in out.
 * @returns Written length, or zero on failure.
 */
size_t axi_ekf_encode(const axi_ekf_log *log, uint8_t *out, size_t capacity);
/** Encode LogRecord: timestamp_ms, level, source, message, collector_dropped.
 */
size_t axi_log_encode(const axi_log_record *record, uint8_t *out,
                      size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
