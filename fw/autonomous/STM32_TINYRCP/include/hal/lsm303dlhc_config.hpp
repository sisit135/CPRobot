// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>

// Set only after measuring the installed sensor and magnetic field with the
// motors/driver operating. The gyro-free backend depends on magnetic heading
// during motion, so a motors-off-only cleanliness check is insufficient.
#ifndef SENSORS_LSM303DLHC_CALIBRATED
#define SENSORS_LSM303DLHC_CALIBRATED 0
#endif
#ifndef SENSORS_LSM303DLHC_MAGNETIC_ENVIRONMENT_VERIFIED
#define SENSORS_LSM303DLHC_MAGNETIC_ENVIRONMENT_VERIFIED 0
#endif
namespace board {
namespace lsm303dlhc {
constexpr uint8_t kAccelAddress7bit = 0x19, kMagAddress7bit = 0x1E;
// Independent mounting/calibration from the MPU/AK8963. Signed, one-based
// source axes map to body [+X forward, +Y left, +Z up]. Installed sensor:
// native +X faces left, +Y faces back, +Z faces up. Verify both on hardware.
constexpr int8_t kAccelBodyAxes[3] = {-2, 1, 3};
constexpr int8_t kMagBodyAxes[3] = {-2, 1, 3};
constexpr float kAccelBiasMps2[3] = {0, 0, 0};
constexpr float kMagHardIronUt[3] = {0, 0, 0};
constexpr float kMagSoftIron[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
constexpr bool kCalibrated = SENSORS_LSM303DLHC_CALIBRATED;
constexpr bool kMagneticEnvironmentVerified =
    SENSORS_LSM303DLHC_MAGNETIC_ENVIRONMENT_VERIFIED;
constexpr uint32_t kAccelMaxAgeUs = 30000, kHeadingMaxGapUs = 50000;
constexpr float kGravityToleranceMps2 = 1.0f;
constexpr float kFieldMinUt = 15.0f, kFieldMaxUt = 100.0f;
constexpr float kMaxYawRateRps = 4.0f;
constexpr float kRateFilterTimeConstantS = 0.04f;
} // namespace lsm303dlhc
} // namespace board
