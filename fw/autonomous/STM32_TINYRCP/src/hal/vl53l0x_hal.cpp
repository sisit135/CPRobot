// SPDX-License-Identifier: GPL-3.0-only

#include "hal/vl53l0x_hal.hpp"
#include "logging/log.hpp"
#include "platform/hal_platform.hpp"
#include "vl53l0x_api.h"
#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

extern "C" void axi_vl53l0x_log_error(const char *function, const char *format,
                                      ...) {
  va_list arguments;
  va_start(arguments, format);
  (void)robot_log::writev(robot_log::Level::ERROR,
                          function ? function : "vl53l0x", format, arguments);
  va_end(arguments);
}

#ifdef VL53L0X_LOG_ENABLE
extern "C" {
uint32_t _trace_level = TRACE_LEVEL_ALL;
}
extern "C" void trace_print_module_function(uint32_t, uint32_t level, uint32_t,
                                            const char *format, ...) {
  robot_log::Level mapped =
      level <= TRACE_LEVEL_ERRORS    ? robot_log::Level::ERROR
      : level == TRACE_LEVEL_WARNING ? robot_log::Level::WARN
      : level == TRACE_LEVEL_INFO    ? robot_log::Level::INFO
      : level == TRACE_LEVEL_DEBUG   ? robot_log::Level::DEBUG
                                     : robot_log::Level::TRACE;
  va_list arguments;
  va_start(arguments, format);
  (void)robot_log::writev(mapped, "vl53l0x", format, arguments);
  va_end(arguments);
}
#endif

namespace tof {
namespace {
struct Channel {
  VL53L0X_Dev_t dev{};
  Statistics stats{};
  bool fresh = false;
  uint16_t range_mm = 0;
  int64_t read_us = 0, warn_log_us = 0, sample_log_us = 0;
};
Config cfg;
Channel channels[3];
bool configured = false, running = false;
int active = -1, previous = 2;
int64_t shot_started_us = 0, next_poll_us = 0;

const char *name(Event event) {
  switch (event) {
  case Event::INIT:
    return "init";
  case Event::READY:
    return "ready";
  case Event::SAMPLE:
    return "sample";
  case Event::INVALID_RANGE:
    return "invalid";
  case Event::API_ERROR:
    return "error";
  case Event::TIMEOUT:
    return "timeout";
  case Event::STOP:
    return "stop";
  default:
    return "config";
  }
}
const char *stage_name(Stage stage) {
  switch (stage) {
  case Stage::BOOT:
    return "boot";
  case Stage::ADDRESS:
    return "address";
  case Stage::DATA_INIT:
    return "data init";
  case Stage::STATIC_INIT:
    return "static init";
  case Stage::SPADS:
    return "reference SPADs";
  case Stage::REF_CAL:
    return "reference calibration";
  case Stage::OFFSET:
    return "offset calibration";
  case Stage::XTALK_RATE:
    return "crosstalk rate";
  case Stage::XTALK_ENABLE:
    return "crosstalk mode";
  case Stage::MODE:
    return "ranging mode";
  case Stage::BUDGET:
    return "timing budget";
  case Stage::GPIO_OFF:
    return "polling GPIO";
  case Stage::CLEAR:
    return "interrupt clear";
  default:
    return "runtime";
  }
}
void emit(unsigned id, Event event, Stage stage = Stage::NONE, int error = 0,
          uint8_t status = 255, uint16_t mm = 0) {
  const LogRecord r{hal_time_us(),
                    static_cast<loc::TofId>(id),
                    event,
                    stage,
                    error,
                    status,
                    mm};
  if (cfg.log) {
    cfg.log(r, cfg.log_context);
    return;
  }
  (void)robot_log::write(
      event == Event::API_ERROR || event == Event::TIMEOUT
          ? robot_log::Level::ERROR
      : event == Event::INVALID_RANGE ? robot_log::Level::WARN
                                      : robot_log::Level::INFO,
      "tof", "id=%u event=%s stage=%u error=%d status=%u range_mm=%u", id,
      name(event), unsigned(stage), error, unsigned(status), unsigned(mm));
}
void isolate(unsigned id) {
  digitalWrite(cfg.sensors[id].xshut_pin, LOW);
  channels[id].stats.online = false;
  channels[id].fresh = false;
}
#if defined(AXI_BUP_I2C_ISOLATION) && AXI_BUP_I2C_ISOLATION
void shutdown_diagnostic() {
  unsigned levels = 0, acknowledgements = 0;
  for (unsigned i = 0; i < 3; ++i)
    levels |= unsigned(digitalRead(cfg.sensors[i].xshut_pin) != LOW) << i;
  const uint8_t addresses[] = {0x29, cfg.sensors[0].address_7bit,
                                cfg.sensors[1].address_7bit,
                                cfg.sensors[2].address_7bit};
  for (unsigned i = 0; i < 4; ++i)
    if (HAL_I2C_IsDeviceReady(cfg.i2c, uint16_t(addresses[i]) << 1, 1,
                             cfg.i2c_timeout_ms) == HAL_OK)
      acknowledgements |= 1u << i;
  robot_log::write(robot_log::Level::INFO, "tof",
                   "all XSHUT low: GPIO mask=%x ACK mask=%x (29/30/31/32)",
                   levels, acknowledgements);
}
#endif
void fault(unsigned id, VL53L0X_Error error, Stage stage) {
  auto &c = channels[id];
  c.stats.last_api_error = error;
  if (error == VL53L0X_ERROR_TIME_OUT)
    ++c.stats.timeouts;
  else
    ++c.stats.api_errors;
  emit(id, error == VL53L0X_ERROR_TIME_OUT ? Event::TIMEOUT : Event::API_ERROR,
       stage, error);
  isolate(id);
  if (active == int(id))
    active = -1;
}
bool step(unsigned id, VL53L0X_Error error, Stage stage) {
  if (error == VL53L0X_ERROR_NONE) {
    if (!channels[id].stats.online)
      robot_log::write(robot_log::Level::INFO, "tof", "id=%u %s ready", id,
                       stage_name(stage));
    return true;
  }
  fault(id, error, stage);
  return false;
}
bool valid(const Config &c) {
  if (!c.i2c || !c.i2c->Instance || c.timing_budget_us < 20000 ||
      c.timing_budget_us > 200000 ||
      c.measurement_timeout_us < c.timing_budget_us + 20000 ||
      c.measurement_timeout_us > 5000000 || !c.sample_max_age_us ||
      c.sample_max_age_us > 5000000 || !c.poll_interval_us ||
      c.poll_interval_us > 100000 || !c.i2c_timeout_ms ||
      c.i2c_timeout_ms > 100 || c.init_timeout_us < 100000 ||
      c.init_timeout_us > 10000000 || !c.runtime_api_timeout_us ||
      c.runtime_api_timeout_us > 1000000 || !c.max_range_mm ||
      c.max_range_mm > 2000 || !c.log_period_us)
    return false;
  for (unsigned i = 0; i < 3; ++i) {
    const auto &s = c.sensors[i];
    if (s.xshut_pin < 0 || s.address_7bit < 0x08 || s.address_7bit > 0x77 ||
        s.address_7bit == 0x29)
      return false;
    for (unsigned j = 0; j < i; ++j)
      if (s.xshut_pin == c.sensors[j].xshut_pin ||
          s.address_7bit == c.sensors[j].address_7bit)
        return false;
  }
  return true;
}
bool setup_one(unsigned i) {
  auto &c = channels[i];
  auto &d = c.dev;
  d = {};
  d.I2cHandle = cfg.i2c;
  d.I2cDevAddr = 0x52;
  d.comms_type = 1;
  d.comms_speed_khz = cfg.comms_speed_khz;
  d.i2c_timeout_ms = cfg.i2c_timeout_ms;
  d.deadline_us = hal_time_us() + cfg.init_timeout_us;
  emit(i, Event::INIT);
  digitalWrite(cfg.sensors[i].xshut_pin,
               HIGH); // Open-drain release, never drive XSHUT high.
  delayMicroseconds(2000);
#if defined(AXI_BUP_I2C_ISOLATION) && AXI_BUP_I2C_ISOLATION
  robot_log::write(robot_log::Level::INFO, "tof",
                   "id=%u XSHUT released: GPIO=%u", i,
                   unsigned(digitalRead(cfg.sensors[i].xshut_pin)));
#endif
  const int64_t boot_end = hal_time_us() + 100000;
  bool booted = false;
  do {
    if (HAL_I2C_IsDeviceReady(cfg.i2c, d.I2cDevAddr, 1, cfg.i2c_timeout_ms) ==
        HAL_OK) {
      booted = true;
      break;
    }
    delayMicroseconds(1000);
  } while (hal_time_us() < boot_end && hal_time_us() < d.deadline_us);
  if (!booted) {
#if defined(AXI_BUP_I2C_ISOLATION) && AXI_BUP_I2C_ISOLATION
    uint8_t model = 0;
    const auto result = HAL_I2C_Mem_Read(
        cfg.i2c, d.I2cDevAddr, 0xc0, I2C_MEMADD_SIZE_8BIT, &model, 1,
        cfg.i2c_timeout_ms);
    robot_log::write(robot_log::Level::INFO, "tof",
                     "id=%u boot probe failed: model read HAL=%u value=%02x",
                     i, unsigned(result), unsigned(model));
#endif
    fault(i, VL53L0X_ERROR_TIME_OUT, Stage::BOOT);
    return false;
  }
  robot_log::write(robot_log::Level::INFO, "tof", "id=%u boot ready", i);
  if (!step(i,
            VL53L0X_SetDeviceAddress(&d,
                                     uint8_t(cfg.sensors[i].address_7bit << 1)),
            Stage::ADDRESS))
    return false;
  d.I2cDevAddr = uint8_t(cfg.sensors[i].address_7bit << 1);
  if (!step(i, VL53L0X_DataInit(&d), Stage::DATA_INIT) ||
      !step(i, VL53L0X_StaticInit(&d), Stage::STATIC_INIT))
    return false;
  uint32_t spads = 0;
  uint8_t aperture = 0, vhv = 0, phase = 0;
  if (!step(i, VL53L0X_PerformRefSpadManagement(&d, &spads, &aperture),
            Stage::SPADS) ||
      !step(i, VL53L0X_PerformRefCalibration(&d, &vhv, &phase), Stage::REF_CAL))
    return false;
  const auto &s = cfg.sensors[i];
  if (s.has_offset_calibration &&
      !step(i, VL53L0X_SetOffsetCalibrationDataMicroMeter(&d, s.offset_um),
            Stage::OFFSET))
    return false;
  if (s.has_xtalk_calibration &&
      !step(i,
            VL53L0X_SetXTalkCompensationRateMegaCps(
                &d, static_cast<FixPoint1616_t>(s.xtalk_mcps_q16)),
            Stage::XTALK_RATE))
    return false;
  if (!step(i,
            VL53L0X_SetXTalkCompensationEnable(&d,
                                               s.has_xtalk_calibration ? 1 : 0),
            Stage::XTALK_ENABLE) ||
      !step(i, VL53L0X_SetDeviceMode(&d, VL53L0X_DEVICEMODE_SINGLE_RANGING),
            Stage::MODE) ||
      !step(i,
            VL53L0X_SetMeasurementTimingBudgetMicroSeconds(
                &d, cfg.timing_budget_us),
            Stage::BUDGET) ||
      !step(i,
            VL53L0X_SetGpioConfig(&d, 0, VL53L0X_DEVICEMODE_SINGLE_RANGING,
                                  VL53L0X_GPIOFUNCTIONALITY_OFF,
                                  VL53L0X_INTERRUPTPOLARITY_LOW),
            Stage::GPIO_OFF) ||
      !step(i, VL53L0X_ClearInterruptMask(&d, 0), Stage::CLEAR))
    return false;
  // ClearInterruptMask acknowledges internal status only; there is no GPIO/ISR
  // path.
  d.deadline_us = 0;
  c.stats.online = true;
  emit(i, Event::READY);
  return true;
}
void start_next() {
  if (!running || active >= 0)
    return;
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    previous = (previous + 1) % 3;
    auto &c = channels[previous];
    if (!c.stats.online)
      continue;
    c.dev.deadline_us = hal_time_us() + cfg.runtime_api_timeout_us;
    const auto result = VL53L0X_StartMeasurement(&c.dev);
    c.dev.deadline_us = 0;
    if (!step(unsigned(previous), result, Stage::START))
      continue;
    active = previous;
    shot_started_us = hal_time_us();
    next_poll_us = shot_started_us + cfg.poll_interval_us;
    return;
  }
}
void service() {
  if (!running)
    return;
  if (active < 0) {
    start_next();
    return;
  }
  const int64_t now = hal_time_us();
  if (now < next_poll_us)
    return;
  next_poll_us = now + cfg.poll_interval_us;
  const unsigned i = unsigned(active);
  auto &c = channels[i];
  if (now - shot_started_us >= cfg.measurement_timeout_us) {
    fault(i, VL53L0X_ERROR_TIME_OUT, Stage::POLL);
    start_next();
    return;
  }
  c.dev.deadline_us = now + cfg.runtime_api_timeout_us;
  uint8_t ready = 0;
  if (!step(i, VL53L0X_GetMeasurementDataReady(&c.dev, &ready), Stage::POLL)) {
    start_next();
    return;
  }
  if (!ready) {
    c.dev.deadline_us = 0;
    return;
  }
  VL53L0X_RangingMeasurementData_t result{};
  if (!step(i, VL53L0X_GetRangingMeasurementData(&c.dev, &result),
            Stage::READ) ||
      !step(i, VL53L0X_ClearInterruptMask(&c.dev, 0), Stage::CLEAR)) {
    start_next();
    return;
  }
  c.dev.deadline_us = 0;
  const int64_t completed = hal_time_us();
  active = -1;
  c.stats.last_range_status = result.RangeStatus;
  // Keep this display-only classification separate from navigation admission.
  // Signal/phase failures with a beyond-limit distance include no-return shots;
  // sigma/min-range/hardware failures must not be displayed as open space.
  c.stats.last_out_of_range =
      result.RangeMilliMeter > cfg.max_range_mm &&
      (result.RangeStatus == 0 || result.RangeStatus == 2 ||
       result.RangeStatus == 4);
  if (result.RangeStatus == 0 && result.RangeMilliMeter > 0 &&
      result.RangeMilliMeter <= cfg.max_range_mm) {
    if (c.fresh)
      ++c.stats.overwritten;
    c.range_mm = result.RangeMilliMeter;
    c.read_us = completed;
    c.fresh = true;
    ++c.stats.valid;
    if (cfg.log_samples && (c.sample_log_us == 0 ||
                            completed - c.sample_log_us >= cfg.log_period_us)) {
      emit(i, Event::SAMPLE, Stage::READ, 0, result.RangeStatus,
           result.RangeMilliMeter);
      c.sample_log_us = completed;
    }
  } else {
    c.fresh = false;
    ++c.stats.rejected;
    if (c.warn_log_us == 0 || completed - c.warn_log_us >= cfg.log_period_us) {
      emit(i, Event::INVALID_RANGE, Stage::READ, 0, result.RangeStatus,
           result.RangeMilliMeter);
      c.warn_log_us = completed;
    }
  }
  start_next();
}
} // namespace
bool initialize(const Config &config) {
  // Old wiring is shut down before accepting a different configuration.
  if (configured)
    stop();
  configured = false;
  cfg = config;
  if (!valid(cfg)) {
    emit(0, Event::BAD_CONFIG);
    return false;
  }
  configured = true;
  running = false;
  active = -1;
  previous = 2;
  for (unsigned i = 0; i < 3; ++i) {
    channels[i] = {};
    // Preload LOW before output enable to avoid a transient release.
    digitalWrite(cfg.sensors[i].xshut_pin, LOW);
    pinMode(cfg.sensors[i].xshut_pin, OUTPUT_OPEN_DRAIN);
  }
  delayMicroseconds(2000);
#if defined(AXI_BUP_I2C_ISOLATION) && AXI_BUP_I2C_ISOLATION
  shutdown_diagnostic();
#endif
  for (unsigned i = 0; i < 3; ++i) {
    if (!setup_one(i)) {
      for (unsigned j = 0; j < 3; ++j)
        isolate(j);
#if defined(AXI_BUP_I2C_ISOLATION) && AXI_BUP_I2C_ISOLATION
      delayMicroseconds(2000);
      shutdown_diagnostic();
#endif
      return false;
    }
  }
  running = true; // The first optical measurement starts on the next read().
  return true;
}
bool read(loc::TofId id, float *distance_m, int64_t *timestamp_us) {
  const unsigned i = static_cast<unsigned>(id);
  if (!distance_m || !timestamp_us || i >= 3 || !running)
    return false;
  service();
  auto &c = channels[i];
  if (!c.stats.online || !c.fresh)
    return false;
  c.fresh = false;
  if (hal_time_us() < c.read_us ||
      hal_time_us() - c.read_us > cfg.sample_max_age_us) {
    ++c.stats.expired;
    return false;
  }
  *distance_m = c.range_mm * 0.001f;
  *timestamp_us = c.read_us;
  return true;
}
void stop() {
  if (!configured)
    return;
  running = false;
  active = -1;
  for (unsigned i = 0; i < 3; ++i) {
    isolate(i);
    emit(i, Event::STOP);
  }
  configured = false;
}
Statistics statistics(loc::TofId id) {
  const unsigned i = static_cast<unsigned>(id);
  return i < 3 ? channels[i].stats : Statistics{};
}
} // namespace tof
bool hal_tof_red_distance_corrected(loc::TofId id, float *distance_m,
                                  int64_t *timestamp_us) {
  return tof::read(id, distance_m, timestamp_us);
}
