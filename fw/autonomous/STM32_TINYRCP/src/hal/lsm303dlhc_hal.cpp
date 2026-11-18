// SPDX-License-Identifier: GPL-3.0-only
#include "hal/imu_selection.hpp"
#if AXI_IMU_LSM303DLHC
#include "hal/lsm303dlhc_config.hpp"
#include "hal/lsm303dlhc_hal.hpp"
#include "logging/log.hpp"
#include "lsm303dlhc/lsm303dlhc.h"
#include "platform/hal_platform.hpp"
#include <cmath>
#include <initializer_list>

namespace {
namespace cfg = board::lsm303dlhc;
constexpr float gravity = 9.80665f;
constexpr uint8_t accel_ctrl1 = LSM303DLHC_ODR_100_HZ | LSM303DLHC_AXES_ENABLE;
constexpr uint8_t accel_ctrl4 = LSM303DLHC_BlockUpdate_Single |
                                LSM303DLHC_HR_ENABLE | LSM303DLHC_FULLSCALE_4G;
imu::Config config;
imu::Statistics stats;
bool io_fault = false, pending_mag = false;
int64_t accel_us = 0, mag_us = 0, heading_us = 0, usable_us = 0;
int64_t error_log_us = 0;
loc::ImuSample acceleration;
loc::MagSample levelled;
float previous_heading = 0, filtered_rate = 0;

void event(imu::Event kind, const char *message) {
  const int64_t now = hal_time_us();
  if (kind == imu::Event::API_ERROR || kind == imu::Event::REJECTED) {
    if (error_log_us > 0 && now >= error_log_us &&
        now - error_log_us < config.error_log_period_us)
      return;
    error_log_us = now;
  }
  if (config.log) {
    config.log({now, kind, 0, message}, config.log_context);
  } else if (!robot_log::write(kind == imu::Event::API_ERROR ||
                                       kind == imu::Event::BAD_CONFIG
                                   ? robot_log::Level::ERROR
                                   : robot_log::Level::INFO,
                               "lsm303dlhc", "%s", message)) {
    ++stats.log_dropped;
  }
}
void reset_heading() {
  heading_us = usable_us = 0;
  filtered_rate = 0;
  pending_mag = false;
}
bool rejected(const char *message) {
  ++stats.rejected;
  reset_heading();
  event(imu::Event::REJECTED, message);
  return false;
}
bool transfer(bool write, uint16_t address, uint8_t reg, uint8_t *data,
              uint16_t count) {
  if (!config.i2c || !data || !count || count > 8 ||
      (address != ACC_I2C_ADDRESS && address != MAG_I2C_ADDRESS)) {
    io_fault = true;
    return false;
  }
  // ST BSP and STM32 HAL both use left-shifted addresses. Only acceleration
  // needs SUB(7)=1 for a multi-byte transfer; MAG increments automatically.
  const uint8_t sub =
      reg | ((address == ACC_I2C_ADDRESS && count > 1) ? 0x80 : 0);
  const auto status =
      write ? HAL_I2C_Mem_Write(config.i2c, address, sub, I2C_MEMADD_SIZE_8BIT,
                                data, count, config.i2c_timeout_ms)
            : HAL_I2C_Mem_Read(config.i2c, address, sub, I2C_MEMADD_SIZE_8BIT,
                               data, count, config.i2c_timeout_ms);
  if (status == HAL_OK)
    return true;
  io_fault = true; // Latch failures through ST's void/value-only IO API.
  ++stats.bus_errors;
  stats.last_hal_error = HAL_I2C_GetError(config.i2c);
  reset_heading();
  event(imu::Event::API_ERROR, "I2C transfer failed");
  return false;
}
bool write_reg(uint16_t address, uint8_t reg, uint8_t value) {
  return transfer(true, address, reg, &value, 1);
}
bool read_regs(uint16_t address, uint8_t reg, uint8_t *data, uint16_t count) {
  return transfer(false, address, reg, data, count);
}
bool verify(uint16_t address, uint8_t reg, uint8_t expected) {
  uint8_t value = 0;
  return read_regs(address, reg, &value, 1) && value == expected;
}
int16_t signed_word(uint8_t high, uint8_t low) {
  const uint16_t u = uint16_t(high) << 8 | low;
  return static_cast<int16_t>(u < 0x8000 ? int32_t(u) : int32_t(u) - 65536);
}
bool finite3(const float *v) {
  return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}
float norm3(const float *v) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
bool valid_axes(const int8_t *axes) {
  int indices[3], sign = 1, inversions = 0;
  for (int i = 0; i < 3; ++i) {
    int a = axes[i];
    if (a < -3 || a > 3 || !a)
      return false;
    if (a < 0) {
      sign = -sign;
      a = -a;
    }
    indices[i] = a;
    for (int j = 0; j < i; ++j) {
      if (indices[j] == a)
        return false;
      inversions += indices[j] > a;
    }
  }
  return sign * ((inversions & 1) ? -1 : 1) == 1;
}
void map_axes(const int8_t *axes, const float *native, float *body) {
  for (int i = 0; i < 3; ++i)
    body[i] = axes[i] > 0 ? native[axes[i] - 1] : -native[-axes[i] - 1];
}
bool valid_calibration() {
  if (!valid_axes(cfg::kAccelBodyAxes) || !valid_axes(cfg::kMagBodyAxes) ||
      !finite3(cfg::kAccelBiasMps2) || !finite3(cfg::kMagHardIronUt))
    return false;
  for (const auto &row : cfg::kMagSoftIron)
    if (!finite3(row))
      return false;
  const auto &m = cfg::kMagSoftIron;
  const float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                    m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                    m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
  return std::isfinite(det) && det > 1e-6f;
}
bool valid_time(int64_t start, int64_t stamp, int64_t last) {
  return start > 0 && stamp >= start && stamp > last &&
         stamp - start <= config.max_read_duration_us;
}
} // namespace

// ST's signatures are unchanged. The board owns the already initialized bus.
extern "C" void COMPASSACCELERO_IO_Init(void) {
  if (!config.i2c)
    io_fault = true;
}
extern "C" void COMPASSACCELERO_IO_ITConfig(void) {} // Polling only.
extern "C" void COMPASSACCELERO_IO_Write(uint16_t address, uint8_t reg,
                                         uint8_t value) {
  (void)write_reg(address, reg, value);
}
extern "C" uint8_t COMPASSACCELERO_IO_Read(uint16_t address, uint8_t reg) {
  uint8_t value = 0;
  (void)read_regs(address, reg, &value, 1);
  return value;
}

namespace imu {
bool initialize(const Config &next) {
  if (stats.online)
    stop();
  config = next;
  stats = {};
  io_fault = false;
  accel_us = mag_us = error_log_us = 0;
  reset_heading();
  if (!config.i2c || !config.i2c_timeout_ms ||
      config.i2c_timeout_ms == UINT32_MAX || !config.max_read_duration_us ||
      !valid_calibration()) {
    event(Event::BAD_CONFIG, "invalid bus, timeout or calibration");
    return false;
  }
  event(Event::INIT, "MAG + ACCEL polling; no gyroscope");
  uint8_t id[3]{};
  // The BSP checks accel WHO_AM_I=0x33. Check all MAG H43 identity bytes too.
  const auto acc_id = LSM303DLHC_AccReadID();
  bool ok = !io_fault && acc_id == I_AM_LMS303DLHC &&
            read_regs(MAG_I2C_ADDRESS, LSM303DLHC_IRA_REG_M, id, 3) &&
            id[0] == 0x48 && id[1] == 0x34 && id[2] == 0x33;
  if (ok) {
    // Stop before changing mode; disable interrupts, FIFO and high-pass paths.
    ok = write_reg(ACC_I2C_ADDRESS, LSM303DLHC_CTRL_REG1_A, 0) &&
         write_reg(MAG_I2C_ADDRESS, LSM303DLHC_MR_REG_M, LSM303DLHC_SLEEP);
    for (uint8_t reg : {LSM303DLHC_CTRL_REG2_A, LSM303DLHC_CTRL_REG3_A,
                        LSM303DLHC_CTRL_REG5_A, LSM303DLHC_CTRL_REG6_A,
                        LSM303DLHC_FIFO_CTRL_REG_A, LSM303DLHC_INT1_CFG_A,
                        LSM303DLHC_INT2_CFG_A, LSM303DLHC_CLICK_CFG_A})
      ok = write_reg(ACC_I2C_ADDRESS, reg, 0) && ok;
    LSM303DLHC_AccInit(uint16_t(accel_ctrl4) << 8 | accel_ctrl1);
    ok = ok && !io_fault &&
         write_reg(MAG_I2C_ADDRESS, LSM303DLHC_CRA_REG_M,
                   LSM303DLHC_ODR_220_HZ) &&
         write_reg(MAG_I2C_ADDRESS, LSM303DLHC_CRB_REG_M,
                   LSM303DLHC_FS_1_3_GA) &&
         write_reg(MAG_I2C_ADDRESS, LSM303DLHC_MR_REG_M,
                   LSM303DLHC_CONTINUOS_CONVERSION) &&
         verify(ACC_I2C_ADDRESS, LSM303DLHC_CTRL_REG1_A, accel_ctrl1) &&
         verify(ACC_I2C_ADDRESS, LSM303DLHC_CTRL_REG4_A, accel_ctrl4) &&
         verify(ACC_I2C_ADDRESS, LSM303DLHC_CTRL_REG5_A, 0) &&
         verify(MAG_I2C_ADDRESS, LSM303DLHC_CRA_REG_M, LSM303DLHC_ODR_220_HZ) &&
         verify(MAG_I2C_ADDRESS, LSM303DLHC_CRB_REG_M, LSM303DLHC_FS_1_3_GA) &&
         verify(MAG_I2C_ADDRESS, LSM303DLHC_MR_REG_M, 0);
    uint8_t discard[6]{};
    ok = ok && read_regs(ACC_I2C_ADDRESS, LSM303DLHC_OUT_X_L_A, discard, 6) &&
         read_regs(MAG_I2C_ADDRESS, LSM303DLHC_OUT_X_H_M, discard, 6);
  }
  if (!ok) {
    event(Event::API_ERROR, "identity, configuration or readback failed");
    stop();
    return false;
  }
  stats.online = stats.magnetometer_online = true;
  event(Event::READY,
        cfg::kCalibrated && cfg::kMagneticEnvironmentVerified
            ? "ready; compass-derived planar yaw rate"
            : "raw sensors ready; compass qualification required");
  return true;
}

bool read_magnetic_native(loc::MagSample *out, int64_t *timestamp_us) {
  if (!out || !timestamp_us || !stats.magnetometer_online)
    return false;
  const int64_t start = hal_time_us();
  uint8_t status = 0, bytes[6]{};
  if (!read_regs(MAG_I2C_ADDRESS, LSM303DLHC_SR_REG_M, &status, 1))
    return false;
  if (!(status & 3)) {
    ++stats.not_ready;
    return false;
  }
  // Read all six bytes even after LOCK/partial failures to release the latch.
  if (!read_regs(MAG_I2C_ADDRESS, LSM303DLHC_OUT_X_H_M, bytes, 6))
    return false;
  if (status & 2) {
    ++stats.mag_overruns;
    return rejected("MAG lock; partial frame drained");
  }
  const int16_t raw[3] = {signed_word(bytes[0], bytes[1]),
                          signed_word(bytes[4], bytes[5]),
                          signed_word(bytes[2], bytes[3])}; // Wire X,Z,Y.
  for (int16_t value : raw) {
    if (value <= -2048 || value >= 2047) {
      ++stats.mag_overflows;
      return rejected("MAG overflow/clipping");
    }
  }
  const int64_t stamp = hal_time_us();
  if (!valid_time(start, stamp, mag_us))
    return rejected("MAG timestamp invalid");
  *out = {raw[0] * (100.0f / LSM303DLHC_M_SENSITIVITY_XY_1_3Ga),
          raw[1] * (100.0f / LSM303DLHC_M_SENSITIVITY_XY_1_3Ga),
          raw[2] * (100.0f / LSM303DLHC_M_SENSITIVITY_Z_1_3Ga)};
  *timestamp_us = mag_us = stamp;
  ++stats.mag_samples;
  return true;
}

bool read(loc::ImuSample *out, int64_t *timestamp_us) {
  if (!out || !timestamp_us || !stats.online)
    return false;
  pending_mag = false;
  loc::ImuSample accel{};
  int64_t stamp = 0;
  (void)lsm303dlhc::read_acceleration(&accel, &stamp);
  loc::MagSample native{};
  if (!read_magnetic_native(&native, &stamp))
    return false;
  if (!cfg::kCalibrated || !cfg::kMagneticEnvironmentVerified ||
      accel_us <= 0 || stamp < accel_us ||
      stamp - accel_us > cfg::kAccelMaxAgeUs)
    return rejected("compass unqualified or acceleration stale");
  const float a[3] = {acceleration.ax_mps2, acceleration.ay_mps2,
                      acceleration.az_mps2};
  if (std::fabs(norm3(a) - gravity) > cfg::kGravityToleranceMps2)
    return rejected("dynamic acceleration; tilt unavailable");
  const float input[3] = {native.mx_uT - cfg::kMagHardIronUt[0],
                          native.my_uT - cfg::kMagHardIronUt[1],
                          native.mz_uT - cfg::kMagHardIronUt[2]};
  float corrected[3]{}, body[3]{};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      corrected[i] += cfg::kMagSoftIron[i][j] * input[j];
  map_axes(cfg::kMagBodyAxes, corrected, body);
  const float magnitude = norm3(body);
  if (!finite3(body) || magnitude < cfg::kFieldMinUt ||
      magnitude > cfg::kFieldMaxUt)
    return rejected("magnetic field outside bounds");
  // AN3192 quasi-static tilt compensation, adapted to body +X/+Y/+Z up.
  const float roll = std::atan2(a[1], a[2]);
  const float pitch = std::atan2(-a[0], std::hypot(a[1], a[2]));
  const float cr = std::cos(roll), sr = std::sin(roll), cp = std::cos(pitch),
              sp = std::sin(pitch);
  levelled = {body[0] * cp + body[1] * sr * sp + body[2] * cr * sp,
              body[1] * cr - body[2] * sr,
              -body[0] * sp + body[1] * sr * cp + body[2] * cr * cp};
  if (std::hypot(levelled.mx_uT, levelled.my_uT) < 1.0f)
    return rejected("vertical field; heading undefined");
  const float heading = std::atan2(-levelled.my_uT, levelled.mx_uT);
  if (heading_us <= 0 || stamp <= heading_us ||
      stamp - heading_us > cfg::kHeadingMaxGapUs) {
    reset_heading();
    previous_heading = heading;
    heading_us = stamp;
    return false; // Seed only; never invent a stationary rate for the first
                  // frame.
  }
  const float dt = float(stamp - heading_us) * 1e-6f;
  const float rate = loc::wrap_pi(heading - previous_heading) / dt;
  if (!std::isfinite(rate) || std::fabs(rate) > cfg::kMaxYawRateRps)
    return rejected("compass heading jump");
  filtered_rate +=
      dt / (cfg::kRateFilterTimeConstantS + dt) * (rate - filtered_rate);
  previous_heading = heading;
  heading_us = usable_us = stamp;
  pending_mag = true;
  auto sample = acceleration;
  sample.gz_rps = filtered_rate; // MAG-derived planar rate, no physical gyro.
  ++stats.imu_samples;
  *out = sample;
  *timestamp_us = stamp;
  return true;
}
bool read_magnetic(loc::MagSample *out, int64_t *timestamp_us) {
  if (!out || !timestamp_us || !pending_mag ||
      !lsm303dlhc::magnetic_heading_usable())
    return false;
  pending_mag = false;
  *out = levelled;
  *timestamp_us = usable_us;
  return true;
}
void stop() {
  stats.online = stats.magnetometer_online = false;
  reset_heading();
  accel_us = mag_us = 0;
  if (config.i2c) {
    (void)write_reg(ACC_I2C_ADDRESS, LSM303DLHC_CTRL_REG1_A, 0);
    (void)write_reg(MAG_I2C_ADDRESS, LSM303DLHC_MR_REG_M, LSM303DLHC_SLEEP);
  }
  event(Event::STOP, "power down; shared bus retained");
}
Statistics statistics() { return stats; }
} // namespace imu

namespace lsm303dlhc {
bool read_acceleration(loc::ImuSample *out, int64_t *timestamp_us) {
  if (!out || !timestamp_us || !stats.online)
    return false;
  const int64_t start = hal_time_us();
  uint8_t bytes[7]{};
  // BDU + one auto-increment burst keeps all axis bytes from one frame.
  if (!read_regs(ACC_I2C_ADDRESS, LSM303DLHC_STATUS_REG_A, bytes, 7)) {
    accel_us = 0;
    return false;
  }
  if (!(bytes[0] & 8)) {
    ++stats.not_ready;
    return false;
  }
  float native[3], body[3];
  for (int i = 0; i < 3; ++i) {
    const int raw = signed_word(bytes[2 + 2 * i], bytes[1 + 2 * i]) / 16;
    if (raw <= -2048 || raw >= 2047) {
      accel_us = 0;
      return rejected("ACCEL clipping");
    }
    native[i] = float(raw) * (0.002f * gravity); // 12-bit +/-4 g: 2 mg/LSB.
  }
  map_axes(cfg::kAccelBodyAxes, native, body);
  for (int i = 0; i < 3; ++i)
    body[i] -= cfg::kAccelBiasMps2[i];
  const int64_t stamp = hal_time_us();
  if (!finite3(body) || !valid_time(start, stamp, accel_us)) {
    accel_us = 0;
    return rejected("ACCEL data/timestamp invalid");
  }
  acceleration = {body[0], body[1], body[2], 0, 0, 0};
  accel_us = *timestamp_us = stamp;
  *out = acceleration;
  return true;
}
bool magnetic_heading_usable() {
  const int64_t now = hal_time_us();
  return stats.online && usable_us > 0 && now >= usable_us &&
         now - usable_us <= cfg::kHeadingMaxGapUs;
}
} // namespace lsm303dlhc
bool hal_imu_read(loc::ImuSample *out, int64_t *stamp) {
  return imu::read(out, stamp);
}
bool hal_mag_read(loc::MagSample *out, int64_t *stamp) {
  return imu::read_magnetic(out, stamp);
}
#endif // AXI_IMU_LSM303DLHC
