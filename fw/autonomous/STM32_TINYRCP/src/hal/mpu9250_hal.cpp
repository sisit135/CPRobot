 // SPDX-License-Identifier: GPL-3.0-only

#include "hal/mpu9250_hal.hpp"
#include "hal/sensors_config.hpp"
#include "logging/log.hpp"
#include "platform/hal_platform.hpp"
#include "remote/settings.hpp"
#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
extern "C" {
#include "driver_mpu9250.h"
}

namespace {
namespace cfg = board::sensors;
constexpr float kGravity = 9.80665f;
constexpr float kRadiansPerDegree = 0.017453292519943295f;
constexpr uint8_t kIntStatusReg = 0x3A;
constexpr uint8_t kWhoAmIReg = 0x75;
constexpr uint8_t kExpectedChipId = 0x71;
constexpr uint8_t kMagSt1Reg = 0x02;
imu::Config config;
imu::Statistics stats;
mpu9250_handle_t device{};
int64_t last_imu_us = 0, last_mag_us = 0, last_error_log_us = 0;
int64_t tilt_us = 0, still_since_us = 0;
float roll = 0, pitch = 0;
struct MagStatus {
  bool captured = false;
  uint8_t st1 = 0, st2 = 0;
} mag_status;

void log_event(imu::Event event, int code, const char *message) {
  const int64_t now = hal_time_us();
  const bool limited =
      event == imu::Event::API_ERROR || event == imu::Event::REJECTED;
  if (limited && last_error_log_us > 0 && now >= last_error_log_us &&
      now - last_error_log_us < config.error_log_period_us)
    return;
  if (limited)
    last_error_log_us = now;
  imu::LogRecord record;
  record.timestamp_us = now;
  record.event = event;
  record.code = code;
  record.message = message;
  if (config.log) {
    config.log(record, config.log_context);
    return;
  }
  auto level = robot_log::Level::INFO;
  if (event == imu::Event::API_ERROR || event == imu::Event::BAD_CONFIG)
    level = robot_log::Level::ERROR;
  else if (event == imu::Event::REJECTED ||
           (event == imu::Event::READY && code != 0))
    level = robot_log::Level::WARN;
  const char *text = message ? message : "";
  if (!robot_log::write(level, "imu", "code=%d %.*s", code,
                        static_cast<int>(std::strcspn(text, "\r\n")), text))
    ++stats.log_dropped;
}

bool api_ok(uint8_t result, const char *operation) {
  if (!result)
    return true;
  log_event(imu::Event::API_ERROR, result, operation);
  return false;
}

uint8_t bus_init() { return config.i2c ? 0 : 1; }
uint8_t bus_deinit() {
  return 0;
} // Shared bus belongs to the board, never end it.
uint8_t spi_unavailable() { return 1; }
uint8_t spi_read_unavailable(uint8_t, uint8_t *, uint16_t) { return 1; }
uint8_t spi_write_unavailable(uint8_t, uint8_t *, uint16_t) { return 1; }
void receive_unused(uint8_t) {} // Required upstream callback, never dispatched.

bool address_allowed(uint8_t address8) {
  // libdriver callbacks receive LEFT-SHIFTED (8-bit) addresses, as does STM32
  // HAL. Do not shift again here; the shared header stores only 7-bit values.
  return address8 == (cfg::kMpu9250Address7bit << 1) ||
         address8 == (cfg::kAk8963Address7bit << 1);
}

uint8_t bus_read(uint8_t address8, uint8_t reg, uint8_t *bytes,
                 uint16_t count) {
  if (!config.i2c || !bytes || count == 0 || count > 32 ||
      !address_allowed(address8))
    return 1;
  const HAL_StatusTypeDef result =
      HAL_I2C_Mem_Read(config.i2c, address8, reg, I2C_MEMADD_SIZE_8BIT, bytes,
                       count, config.i2c_timeout_ms);
  if (result != HAL_OK) {
    ++stats.bus_errors;
    stats.last_hal_error = HAL_I2C_GetError(config.i2c);
    if (address8 == (cfg::kMpu9250Address7bit << 1) && reg == kWhoAmIReg &&
        count == 1)
      robot_log::write(robot_log::Level::DEBUG, "imu",
                       "CHIPID addr=0x%02x reg=0x%02x read failed status=%u error=0x%lx",
                       unsigned(address8 >> 1), unsigned(reg), unsigned(result),
                       static_cast<unsigned long>(stats.last_hal_error));
    return 1;
  }
  // Observe the driver's identity read, including mismatches before init fails.
  // No extra I2C transaction or change to the vendor's identity validation.
  if (address8 == (cfg::kMpu9250Address7bit << 1) && reg == kWhoAmIReg &&
      count == 1)
    robot_log::write(robot_log::Level::DEBUG, "imu",
                     "CHIPID addr=0x%02x WHO_AM_I=0x%02x expected=0x%02x %s",
                     unsigned(address8 >> 1), unsigned(bytes[0]),
                     unsigned(kExpectedChipId),
                     bytes[0] == kExpectedChipId ? "match" : "mismatch");
  // Inspect status from THE SAME burst consumed by libdriver's mag converter.
  // A separate read of ST2 would unlock/clear the data before validation.
  if (address8 == (cfg::kAk8963Address7bit << 1) && reg == kMagSt1Reg &&
      count == 8) {
    mag_status.captured = true;
    mag_status.st1 = bytes[0];
    mag_status.st2 = bytes[7];
  }
  return 0;
}

uint8_t bus_write(uint8_t address8, uint8_t reg, uint8_t *bytes,
                  uint16_t count) {
  if (!config.i2c || !bytes || count == 0 || count > 32 ||
      !address_allowed(address8))
    return 1;
  const HAL_StatusTypeDef result =
      HAL_I2C_Mem_Write(config.i2c, address8, reg, I2C_MEMADD_SIZE_8BIT, bytes,
                        count, config.i2c_timeout_ms);
  if (result != HAL_OK) {
    ++stats.bus_errors;
    stats.last_hal_error = HAL_I2C_GetError(config.i2c);
    return 1;
  }
  return 0;
}

void driver_delay(uint32_t milliseconds) {
#if defined(INCLUDE_xTaskGetSchedulerState) &&                                 \
    (INCLUDE_xTaskGetSchedulerState == 1)
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    TickType_t ticks = pdMS_TO_TICKS(milliseconds);
    if (ticks == 0 && milliseconds)
      ticks = 1;
    vTaskDelay(ticks);
    return;
  }
#endif
  delay(milliseconds); // Also valid before the scheduler starts.
}

void driver_debug(const char *const format, ...) {
  char text[96];
  va_list args;
  va_start(args, format);
  std::vsnprintf(text, sizeof(text), format, args);
  va_end(args);
#ifdef AXI_VENDOR_TRACE_ENABLE
  // Vendor bursts must not suppress the HAL's subsequent API failure report.
  if (!robot_log::write(robot_log::Level::TRACE, "mpu9250", "%.*s",
                        static_cast<int>(std::strcspn(text, "\r\n")), text))
    ++stats.log_dropped;
#else
  log_event(imu::Event::API_ERROR, -1, text);
#endif
}

bool valid_axes(const int8_t axes[3]) {
  int indices[3], sign = 1, inversions = 0;
  for (int i = 0; i < 3; ++i) {
    int axis = axes[i];
    if (axis == 0 || axis < -3 || axis > 3)
      return false;
    if (axis < 0) {
      sign = -sign;
      axis = -axis;
    }
    indices[i] = axis;
    for (int j = 0; j < i; ++j) {
      if (indices[j] == axis)
        return false;
      if (indices[j] > axis)
        ++inversions;
    }
  }
  return sign * ((inversions & 1) ? -1 : 1) ==
         1; // Proper rotation, not reflection.
}

void map_axes(const int8_t axes[3], const float input[3], float output[3]) {
  for (int i = 0; i < 3; ++i) {
    const int a = axes[i];
    output[i] = a > 0 ? input[a - 1] : -input[-a - 1];
  }
}

float norm3(const float v[3]) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
bool finite3(const float v[3]) {
  return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}
int16_t signed_be(const uint8_t *p) {
  const uint16_t u = (uint16_t(p[0]) << 8) | p[1];
  return static_cast<int16_t>(u < 0x8000 ? int32_t(u) : int32_t(u) - 65536);
}

void update_tilt(const float accel[3], const float gyro[3], int64_t stamp) {
  const bool stable =
      std::fabs(norm3(accel) - kGravity) <= cfg::kTiltGravityToleranceMps2 &&
      norm3(gyro) <= cfg::kTiltMaxGyroRps;
  if (!stable || (tilt_us > 0 && (stamp <= tilt_us ||
                                  stamp - tilt_us > cfg::kMagTiltMaxAgeUs))) {
    tilt_us = 0;
    still_since_us = 0;
  }
  if (!stable)
    return;
  if (still_since_us == 0)
    still_since_us = stamp;
  roll = std::atan2(accel[1], accel[2]);
  pitch = std::atan2(-accel[0],
                     std::sqrt(accel[1] * accel[1] + accel[2] * accel[2]));
  tilt_us = stamp;
}

bool valid_calibration() {
  if (!valid_axes(cfg::kImuBodyAxes) || !valid_axes(cfg::kMagBodyAxes) ||
      !finite3(cfg::kAccelBiasMps2) || !finite3(cfg::kGyroBiasRps) ||
      !finite3(cfg::kMagHardIronUt))
    return false;
  for (int i = 0; i < 3; ++i)
    if (!finite3(cfg::kMagSoftIron[i]))
      return false;
  const auto &m = cfg::kMagSoftIron;
  const float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                    m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                    m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
  return std::isfinite(det) && det > 1e-6f;
}

bool verify_imu_configuration() {
  uint8_t rates[2]{}, ranges[3]{}, interrupt_enable = 0xFF, user_ctrl = 0xFF;
  if (!api_ok(mpu9250_get_reg(&device, 0x19, rates, 2),
              "read back rate/filter") ||
      !api_ok(mpu9250_get_reg(&device, 0x1B, ranges, 3), "read back ranges") ||
      !api_ok(mpu9250_get_reg(&device, 0x38, &interrupt_enable, 1),
              "read back INT_ENABLE") ||
      !api_ok(mpu9250_get_reg(&device, 0x6A, &user_ctrl, 1),
              "read back USER_CTRL"))
    return false;
  const bool matches = rates[0] == 9 && (rates[1] & 0x3F) == 3 &&
                       (ranges[0] & 0xFB) == 0x08 &&
                       (ranges[1] & 0xF8) == 0x08 && (ranges[2] & 0x0F) == 3 &&
                       interrupt_enable == 0 && (user_ctrl & 0xE0) == 0;
  if (!matches)
    log_event(imu::Event::API_ERROR, -2, "configuration readback mismatch");
  return matches;
}
} // namespace

namespace imu {
bool initialize(const Config &settings) {
  if (device.inited)
    stop();
  config = settings;
  stats = Statistics{};
  device = mpu9250_handle_t{};
  mag_status = MagStatus{};
  last_imu_us = last_mag_us = last_error_log_us = tilt_us = still_since_us = 0;
  if (!config.i2c || !config.i2c->Instance || config.i2c_timeout_ms == 0 ||
      config.i2c_timeout_ms > 100 || config.max_read_duration_us == 0 ||
      config.max_read_duration_us > 100000 || !valid_calibration()) {
    log_event(Event::BAD_CONFIG, -1,
              "I2C handle, timeout or axis/calibration config invalid");
    return false;
  }
  log_event(Event::INIT, 0, "MPU9250 I2C polling init");
  DRIVER_MPU9250_LINK_INIT(&device, mpu9250_handle_t);
  DRIVER_MPU9250_LINK_IIC_INIT(&device, bus_init);
  DRIVER_MPU9250_LINK_IIC_DEINIT(&device, bus_deinit);
  DRIVER_MPU9250_LINK_IIC_READ(&device, bus_read);
  DRIVER_MPU9250_LINK_IIC_WRITE(&device, bus_write);
  DRIVER_MPU9250_LINK_SPI_INIT(&device, spi_unavailable);
  DRIVER_MPU9250_LINK_SPI_DEINIT(&device, spi_unavailable);
  DRIVER_MPU9250_LINK_SPI_READ(&device, spi_read_unavailable);
  DRIVER_MPU9250_LINK_SPI_WRITE(&device, spi_write_unavailable);
  DRIVER_MPU9250_LINK_DELAY_MS(&device, driver_delay);
  DRIVER_MPU9250_LINK_DEBUG_PRINT(&device, driver_debug);
  DRIVER_MPU9250_LINK_RECEIVE_CALLBACK(&device, receive_unused);
#define IMU_CONFIGURE(call)                                                    \
  do {                                                                         \
    if (!api_ok((call), #call)) {                                              \
      stop();                                                                  \
      return false;                                                            \
    }                                                                          \
  } while (0)
  IMU_CONFIGURE(mpu9250_set_interface(&device, MPU9250_INTERFACE_IIC));
  IMU_CONFIGURE(mpu9250_set_addr_pin(&device, cfg::kMpu9250Address7bit == 0x68
                                                  ? MPU9250_ADDRESS_AD0_LOW
                                                  : MPU9250_ADDRESS_AD0_HIGH));
  IMU_CONFIGURE(mpu9250_init(&device)); // Checks WHO_AM_I == 0x71, then resets.
  IMU_CONFIGURE(mpu9250_set_sleep(&device, MPU9250_BOOL_FALSE));
  driver_delay(100);
  IMU_CONFIGURE(mpu9250_set_clock_source(&device, MPU9250_CLOCK_SOURCE_PLL));
  IMU_CONFIGURE(mpu9250_set_cycle_wake_up(&device, MPU9250_BOOL_FALSE));
  IMU_CONFIGURE(mpu9250_set_gyro_standby(&device, MPU9250_BOOL_FALSE));
  const mpu9250_source_t sources[] = {
      MPU9250_SOURCE_ACC_X,  MPU9250_SOURCE_ACC_Y,  MPU9250_SOURCE_ACC_Z,
      MPU9250_SOURCE_GYRO_X, MPU9250_SOURCE_GYRO_Y, MPU9250_SOURCE_GYRO_Z};
  for (auto source : sources)
    IMU_CONFIGURE(
        mpu9250_set_standby_mode(&device, source, MPU9250_BOOL_FALSE));
  IMU_CONFIGURE(mpu9250_set_fifo(&device, MPU9250_BOOL_FALSE));
  IMU_CONFIGURE(mpu9250_set_iic_master(&device, MPU9250_BOOL_FALSE));
  IMU_CONFIGURE(
      mpu9250_set_gyroscope_range(&device, MPU9250_GYROSCOPE_RANGE_500DPS));
  IMU_CONFIGURE(
      mpu9250_set_accelerometer_range(&device, MPU9250_ACCELEROMETER_RANGE_4G));
  IMU_CONFIGURE(mpu9250_set_gyroscope_choice(&device, 0));
  IMU_CONFIGURE(
      mpu9250_set_low_pass_filter(&device, MPU9250_LOW_PASS_FILTER_3));
  IMU_CONFIGURE(mpu9250_set_accelerometer_choice(&device, 0));
  IMU_CONFIGURE(mpu9250_set_accelerometer_low_pass_filter(
      &device, MPU9250_ACCELEROMETER_LOW_PASS_FILTER_3));
  static_assert(cfg::kImuSampleRateHz == 100,
                "Review read scaling/timing if changing ODR");
  IMU_CONFIGURE(mpu9250_set_sample_rate_divider(&device, 9));
  // Reset already disables DMP. Explicitly clear the entire INT_ENABLE byte.
  uint8_t zero = 0;
  IMU_CONFIGURE(mpu9250_set_reg(&device, 0x38, &zero, 1));
  IMU_CONFIGURE(mpu9250_set_interrupt_read_clear(&device, MPU9250_BOOL_FALSE));
  IMU_CONFIGURE(mpu9250_set_fsync_interrupt(&device, MPU9250_BOOL_FALSE));
  if (!verify_imu_configuration()) {
    stop();
    return false;
  }
  stats.online = true;
  // AK8963 failure is non-fatal to accel/gyro. Only this dedicated mag read
  // consumes magnetometer data; the IMU burst never calls the combined read.
  const bool mag_initialized = api_ok(mpu9250_mag_init(&device), "AK8963 init");
  const bool asa_valid = device.mag_asa[0] != 0 && device.mag_asa[0] != 255 &&
                         device.mag_asa[1] != 0 && device.mag_asa[1] != 255 &&
                         device.mag_asa[2] != 0 && device.mag_asa[2] != 255;
  if (mag_initialized && !asa_valid)
    log_event(Event::API_ERROR, -3, "invalid AK8963 factory ASA");
  if (mag_initialized && asa_valid &&
      api_ok(mpu9250_mag_set_bits(&device, MPU9250_MAGNETOMETER_BITS_16),
             "AK8963 16-bit")) {
    driver_delay(1); // AK8963 mode transition guard while still powered down.
    stats.magnetometer_online = api_ok(
        mpu9250_mag_set_mode(&device, MPU9250_MAGNETOMETER_MODE_CONTINUOUS2),
        "AK8963 continuous 100 Hz");
  }
  if (!stats.magnetometer_online && device.mag_inited)
    (void)mpu9250_mag_deinit(&device);
  driver_delay(10);
  log_event(Event::READY, stats.magnetometer_online ? 0 : 1,
            stats.magnetometer_online
                ? "gyro ready; mag fusion requires calibration"
                : "gyro ready; magnetometer unavailable");
#undef IMU_CONFIGURE
  return true;
}

bool read(loc::ImuSample *out, int64_t *timestamp_us) {
  if (!out || !timestamp_us || !stats.online)
    return false;
  uint8_t bytes[15]{};
  const int64_t started = hal_time_us();
  // One contiguous status + accel/temp/gyro burst. Serial-interface shadow
  // registers keep the frame coherent; do not poll INT_STATUS in a separate
  // transaction and accidentally accept the same subsequent frame twice.
  if (!api_ok(mpu9250_get_reg(&device, kIntStatusReg, bytes, sizeof(bytes)),
              "IMU burst")) {
    tilt_us = still_since_us = 0;
    return false;
  }
  if (!(bytes[0] & 1)) {
    ++stats.not_ready;
    return false;
  }
  const int64_t stamp = hal_time_us();
  if (started <= 0 || stamp <= last_imu_us || stamp < started ||
      stamp - started > config.max_read_duration_us) {
    ++stats.rejected;
    return false;
  }
  float native_accel[3], native_gyro[3], accel[3], gyro[3];
  for (int i = 0; i < 3; ++i) {
    const int16_t a = signed_be(bytes + 1 + i * 2),
                  g = signed_be(bytes + 9 + i * 2);
    if (a <= -32760 || a >= 32760 || g <= -32760 || g >= 32760) {
      ++stats.rejected;
      tilt_us = still_since_us = 0;
      log_event(Event::REJECTED, 1, "accel/gyro clipping");
      return false;
    }
    // Ranges above are fixed: +/-4g => 8192 LSB/g; +/-500 => 65.5 LSB/dps.
    native_accel[i] = float(a) * (kGravity / 8192.0f);
    native_gyro[i] = float(g) * (kRadiansPerDegree / 65.5f);
  }
  map_axes(cfg::kImuBodyAxes, native_accel, accel);
  map_axes(cfg::kImuBodyAxes, native_gyro, gyro);
  for (int i = 0; i < 3; ++i) {
    accel[i] -= cfg::kAccelBiasMps2[i];
    gyro[i] -= cfg::kGyroBiasRps[i];
  }
  gyro[2] -= remote::gyro_zero();
  if (!finite3(accel) || !finite3(gyro)) {
    ++stats.rejected;
    return false;
  }
  update_tilt(accel, gyro, stamp);
  loc::ImuSample sample{};
  sample.ax_mps2 = accel[0];
  sample.ay_mps2 = accel[1];
  sample.az_mps2 = accel[2];
  sample.gx_rps = gyro[0];
  sample.gy_rps = gyro[1];
  sample.gz_rps = gyro[2];
  last_imu_us = stamp;
  ++stats.imu_samples;
  *out = sample;
  *timestamp_us = stamp;
  return true;
}

bool read_magnetic_native(loc::MagSample *out, int64_t *timestamp_us) {
  if (!out || !timestamp_us || !stats.online || !stats.magnetometer_online)
    return false;
  int16_t raw[3]{};
  float field[3]{};
  mag_status = MagStatus{};
  const int64_t started = hal_time_us();
  if (!api_ok(mpu9250_mag_read(&device, raw, field), "AK8963 burst"))
    return false;
  const int64_t stamp = hal_time_us();
  if (!mag_status.captured) {
    ++stats.rejected;
    return false;
  }
  if (mag_status.st1 & MPU9250_MAGNETOMETER_STATUS1_DOR)
    ++stats.mag_overruns;
  if (mag_status.st2 & MPU9250_MAGNETOMETER_STATUS2_HOFL)
    ++stats.mag_overflows;
  if (!(mag_status.st1 & MPU9250_MAGNETOMETER_STATUS1_DRDY)) {
    ++stats.not_ready;
    return false;
  }
  if ((mag_status.st1 & MPU9250_MAGNETOMETER_STATUS1_DOR) ||
      (mag_status.st2 & MPU9250_MAGNETOMETER_STATUS2_HOFL) ||
      !(mag_status.st2 & MPU9250_MAGNETOMETER_STATUS2_BITM) ||
      !finite3(field) || started <= 0 || stamp <= last_mag_us ||
      stamp < started || stamp - started > config.max_read_duration_us) {
    ++stats.rejected;
    log_event(Event::REJECTED, 2, "mag status/clock invalid");
    return false;
  }
  loc::MagSample sample{};
  sample.mx_uT = field[0];
  sample.my_uT = field[1];
  sample.mz_uT = field[2];
  last_mag_us = stamp;
  ++stats.mag_samples;
  *out = sample;
  *timestamp_us = stamp;
  return true;
}

bool read_magnetic(loc::MagSample *out, int64_t *timestamp_us) {
  if (!out || !timestamp_us)
    return false;
  loc::MagSample native{};
  int64_t stamp = 0;
  // Drain even when uncalibrated or moving: never replay old magnetic data.
  if (!read_magnetic_native(&native, &stamp))
    return false;
  if (!cfg::kMagCalibrationAndAxesVerified || tilt_us <= 0 || stamp < tilt_us ||
      stamp - tilt_us > cfg::kMagTiltMaxAgeUs || still_since_us <= 0 ||
      stamp - still_since_us < cfg::kTiltSettleUs)
    return false;
  float input[3] = {native.mx_uT - cfg::kMagHardIronUt[0],
                    native.my_uT - cfg::kMagHardIronUt[1],
                    native.mz_uT - cfg::kMagHardIronUt[2]};
  float corrected[3]{}, body[3];
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      corrected[i] += cfg::kMagSoftIron[i][j] * input[j];
  map_axes(cfg::kMagBodyAxes, corrected, body);
  const float magnitude = norm3(body);
  if (!finite3(body) || magnitude < cfg::kMagFieldMinUt ||
      magnitude > cfg::kMagFieldMaxUt) {
    ++stats.rejected;
    log_event(Event::REJECTED, 3, "mag field outside bounds");
    return false;
  }
  const float cr = std::cos(roll), sr = std::sin(roll), cp = std::cos(pitch),
              sp = std::sin(pitch);
  // R_y(pitch)*R_x(roll) removes tilt while preserving yaw in the +X/+Y plane.
  loc::MagSample sample{};
  sample.mx_uT = body[0] * cp + body[1] * sr * sp + body[2] * cr * sp;
  sample.my_uT = body[1] * cr - body[2] * sr;
  sample.mz_uT = -body[0] * sp + body[1] * sr * cp + body[2] * cr * cp;
  if (sample.mx_uT * sample.mx_uT + sample.my_uT * sample.my_uT < 1.0f) {
    ++stats.rejected;
    return false; // Heading undefined for near-vertical field.
  }
  *out = sample;
  *timestamp_us = stamp;
  return true;
}

void stop() {
  stats.online = stats.magnetometer_online = false;
  tilt_us = still_since_us = 0;
  if (device.mag_inited)
    (void)mpu9250_mag_deinit(&device);
  if (device.inited)
    (void)mpu9250_deinit(&device);
  device.inited = device.mag_inited = 0;
  log_event(Event::STOP, 0, "sleep; shared I2C bus retained");
}
Statistics statistics() { return stats; }
} // namespace imu

bool hal_imu_read(loc::ImuSample *out, int64_t *stamp) {
  return imu::read(out, stamp);
}
bool hal_mag_read(loc::MagSample *out, int64_t *stamp) {
  return imu::read_magnetic(out, stamp);
}
