// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "localization/types.hpp"
#include "stm32f4xx_hal.h"
#include <cstdint>

/* HAL-only integration for Black Pill F411 / Arduino STM32.
 *
 * References:
 *   https://www.st.com/en/embedded-software/stsw-img005.html
 *   ST UM2039 Rev 2.0, sections 4-6 (November 2022).
 *
 * Uses the bundled src/hal/vendor/vl53l0x_class.cpp through
 * vl53l0x_vendor_api.cpp. Keep vendor notices; do not compile a second external
 * ST C API implementation. Include this project's include/ and src/hal/vendor/;
 * compile vl53l0x_platform.cpp. The supplied vendor notices and license are
 * retained.
 *
 * IMPORTANT: TCS34725 and a reset VL53L0X both use 7-bit 0x29. Use a dedicated
 * ToF I2C controller or external bus isolation during address assignment.
 * Example in the existing hal_board_sensors_init() after initializing that bus:
 *
 *   tof::Config c;
 *   c.i2c = tofWire.getHandle(); // pre-initialized dedicated TwoWire instance
 *   c.sensors[0].xshut_pin = YOUR_LEFT_XSHUT_PIN;
 *   c.sensors[1].xshut_pin = YOUR_FRONT_XSHUT_PIN;
 *   c.sensors[2].xshut_pin = YOUR_RIGHT_XSHUT_PIN;
 *   return tof::initialize(c);
 *
 * The supplied I2C handle/bus object must remain alive for the entire run.
 * Offset/cover-glass crosstalk values must come from per-module calibration;
 * no such values are invented here. Factory offset is retained unless the
 * caller supplies an offset; crosstalk correction is off unless supplied.
 * Initialization recomputes reference SPAD/temperature calibration only.
 *
 * board_hooks.cpp initializes this HAL on its dedicated, configured TwoWire.
 * This HAL supplies the sensor-task hook hal_tof_reddistancecorrected().
 * Use the existing sensor task as the ONLY bus owner for initialize/read/stop.
 * The HAL creates no tasks and registers no ISR. GPIO1 is not connected/used.
 * XSHUT is open-drain; the module must pull it up to its permitted logic rail.
 * Serial logging uses an already initialized Serial; no Serial.begin() is done.
 * A custom structured LogSink should enqueue/copy immediately, never block.
 * Sequential acquisition changes each sensor's update period: size consumer
 * freshness gates for the full three-sensor scan plus sensor-task poll latency.
 * Do not assume a 33 ms per-sensor update period from a 33 ms timing budget.
 *
 * Host validation is documented in docs/RA_TIMEOUT_FALLBACK_validation.md.
 * Physical sensor/STM32 timing still requires target validation.
 */
namespace tof {
enum class Event : uint8_t {
  INIT,
  READY,
  SAMPLE,
  INVALID_RANGE,
  API_ERROR,
  TIMEOUT,
  STOP,
  BAD_CONFIG
};
enum class Stage : uint8_t {
  NONE,
  BOOT,
  ADDRESS,
  DATA_INIT,
  STATIC_INIT,
  SPADS,
  REF_CAL,
  OFFSET,
  XTALK_RATE,
  XTALK_ENABLE,
  MODE,
  BUDGET,
  GPIO_OFF,
  CLEAR,
  START,
  POLL,
  READ
};
struct LogRecord {
  int64_t timestamp_us = 0;
  loc::TofId sensor = loc::TofId::LEFT;
  Event event = Event::INIT;
  Stage stage = Stage::NONE;
  int api_error = 0;
  uint8_t range_status = 255;
  uint16_t range_mm = 0;
};
using LogSink = void (*)(const LogRecord &record, void *context);
struct SensorConfig {
  int xshut_pin = -1;
  uint8_t address_7bit = 0; // Config constructor assigns 0x30,0x31,0x32.
  bool has_offset_calibration = false;
  int32_t offset_um = 0;
  bool has_xtalk_calibration = false;
  uint32_t xtalk_mcps_q16 =
      0; // ST FixPoint1616_t, measured using ST procedure.
};
struct Config {
  I2C_HandleTypeDef *i2c = nullptr; // Caller initializes bus/pins/speed.
  SensorConfig sensors[3]{};        // LEFT, FRONT, RIGHT.
  uint32_t timing_budget_us = 33000;
  uint32_t measurement_timeout_us = 150000;
  uint32_t sample_max_age_us = 150000;
  uint32_t poll_interval_us = 2000;
  uint32_t i2c_timeout_ms = 5; // Finite synchronous HAL transfer timeout.
  uint32_t init_timeout_us = 3000000; // Per device, including ST calibration.
  uint32_t runtime_api_timeout_us = 20000;
  uint16_t comms_speed_khz = 100; // Informational; caller owns I2C setup.
  uint16_t max_range_mm = 2000;
  uint32_t log_period_us = 1000000;
  bool log_samples = false;
  LogSink log = nullptr; // nullptr selects compact best-effort Serial.
  void *log_context = nullptr;
  Config() {
    sensors[0].address_7bit = 0x30;
    sensors[1].address_7bit = 0x31;
    sensors[2].address_7bit = 0x32;
  }
};
struct Statistics {
  uint32_t valid = 0, rejected = 0, api_errors = 0, timeouts = 0;
  uint32_t overwritten = 0, expired = 0;
  uint8_t last_range_status = 255;
  bool last_out_of_range = false; // Latest completed shot, diagnostic only.
  int last_api_error = 0;
  bool online = false;
};
/** @brief Reset/address/configure three VL53L0X sensors and prepare polling.
 * @param[in] config Initialized I2C handle, real XSHUT pins and optional
 * calibration.
 * @returns true only when all three devices initialize successfully.
 * @note Startup may block for ST calibration. ST calibration uses internally
 * polled status flags; no MCU interrupt handler is installed. Runtime
 * explicitly selects GPIOFUNCTIONALITY_OFF, SINGLE_RANGING and host readiness
 * polling.
 */
bool initialize(const Config &config);
/** @brief Advance sequential ranging and consume one fresh valid sample.
 * @param[in] id LEFT, FRONT or RIGHT.
 * @param[out] distance_m Corrected optical range in meters; unchanged on false.
 * @param[out] timestamp_us Shared hal_time_us() read-completion time, not an
 * estimated integration-start timestamp; unchanged on false.
 * @returns true once per new RangeStatus==0 measurement, false otherwise.
 * @note Does not wait for optical integration. Synchronous I2C and the ST
 * start handshake can consume time; this is not a hard-WCET guarantee. Failed
 * channels are XSHUT-isolated until explicit reinitialization.
 */
bool read(loc::TofId id, float *distance_m, int64_t *timestamp_us);
/** @brief Hold all configured devices in XSHUT and invalidate pending samples.
 * @returns No value. Call only from the bus-owner/startup context.
 */
void stop();
/** @brief Copy per-device diagnostic counters without bus access.
 * @param[in] id Sensor index.
 * @returns Snapshot; a zero/default snapshot for an invalid index.
 * @note Read in the same owner context, or externally synchronize access.
 */
Statistics statistics(loc::TofId id);
} // namespace tof
/** @brief Existing robot sensor-task adapter; spelling is intentionally
 * preserved.
 * @param[in] id LEFT, FRONT or RIGHT.
 * @param[out] distance_m Fresh valid corrected optical range in meters.
 * @param[out] timestamp_us Read-completion time in the shared monotonic domain.
 * @returns true for one fresh sample; false while waiting, expired or faulted.
 */
bool hal_tof_red_distance_corrected(loc::TofId id, float *distance_m,
                                  int64_t *timestamp_us);
