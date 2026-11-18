// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "hal/imu_hal.hpp"

namespace lsm303dlhc {
// Fresh body acceleration in m/s^2, including gravity, for diagnostics.
// Gyro fields are zero placeholders and must not be used as measurements.
// This consumes the same accelerometer stream as imu::read().
bool read_acceleration(loc::ImuSample *out, int64_t *timestamp_us);
// True only for a fresh calibrated compass field in a verified environment.
// No bus access; call from the sole startup/sensor owner.
bool magnetic_heading_usable();
} // namespace lsm303dlhc
