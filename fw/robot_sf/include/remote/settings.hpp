// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "navigation/feature_navigation.hpp"
#include "navigation/right_hand_rule.hpp"
#include <cstdint>
namespace remote {
// Navigation settings are read under the start/configuration mutex, before
// starting navigation. Once started, mutations are denied until reset.
nav::FeatureConfig feature_settings();
nav::RightHandConfig fallback_settings();
// Sensor-owner tasks consume scalar atomic publications; no extra bus client.
float gyro_zero();
bool tape_thresholds(uint16_t *black_max, uint16_t *white_min);
} // namespace remote
