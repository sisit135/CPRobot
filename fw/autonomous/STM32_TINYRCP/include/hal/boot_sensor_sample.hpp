// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "localization/color_tracker.hpp"
#include "localization/types.hpp"

namespace boot_sensors {
enum class RangeState : uint8_t { UNAVAILABLE, VALID, OUT_OF_RANGE };
struct Snapshot {
  loc::ImuSample imu{};
  loc::MagSample mag{};
  loc::RgbcSample color{};
  bool imu_valid = false, mag_valid = false, color_valid = false;
  RangeState range_state[3]{};
  float range_m[3]{};
};
// Sole BUP bus owner, after HAL init attempt and before starting services.
// Poll for at most 500 ms plus in-flight bounded HAL transfers/scheduling.
void capture(Snapshot &out);
// Only formats the retained snapshot; never accesses a sensor or shared bus.
void print(const Snapshot &sample);
} // namespace boot_sensors
