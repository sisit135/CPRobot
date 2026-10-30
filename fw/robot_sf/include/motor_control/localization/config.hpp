#pragma once
#include "localization/types.hpp"

namespace loc::cfg {
constexpr int grid_n = 5;
constexpr float cell_m = 0.20f;
constexpr float map_w_m = grid_n * cell_m, map_h_m = grid_n * cell_m;
constexpr float initial_x_m = 0.50f, initial_y_m = 0.50f;
constexpr float initial_yaw_rad = 0;
constexpr LidarMount lidar_mounts[3] = {
    {0, +0.06f, +kPi / 2}, {+0.07f, 0, 0}, {0, -0.06f, -kPi / 2}};
constexpr float color_x_body_m = 0.0f, color_y_body_m = 0.0f;

// These are starting values, not sensor specifications.
constexpr float sigma_position_rw = 0.12f; // m / sqrt(s): unknown translation.
constexpr float sigma_gyro_rw = deg2rad(2.0f); // rad / sqrt(s).
constexpr float sigma_line_m =
    0.015f; // Includes stripe width, spot size and latency.
constexpr float sigma_bias_rw = deg2rad(0.15f); // rad/s / sqrt(s).
constexpr float sigma_mag_rad = deg2rad(8.0f), sigma_tof_m = 0.02f;
constexpr float mag_heading_offset_rad = 0; // World bearing of local field.
constexpr float mag_min_uT = 15, mag_max_uT = 100;
constexpr float min_marker_confidence = 0.6f;
constexpr float tof_min_m = 0.015f, tof_max_m = 1.50f;
constexpr float corner_range_gap_m = 0.02f;

constexpr uint32_t sensor_period_ms = 10; // IMU and fresh MAG polling: 100 Hz.
constexpr int64_t tof_period_us = 50000;  // L/F/R polling: 20 Hz each.
constexpr int64_t marker_period_us =
    10000; // Poll conversion state; not a new sample rate.
constexpr uint32_t publish_period_ms = 20, logger_period_ms = 200;
constexpr int event_queue_length = 64;
constexpr int64_t max_event_age_us = 150000;
constexpr int64_t max_imu_hold_us = 50000;
constexpr int64_t max_predict_gap_us = 200000;
constexpr int64_t tracking_imu_age_us = 100000;
constexpr int64_t tracking_fix_age_us = 500000;
constexpr uint32_t sensor_stack_words = 1024, fusion_stack_words = 2048;
constexpr uint32_t logger_stack_words = 768;
constexpr unsigned sensor_priority = 2, fusion_priority = 3,
                   logger_priority = 1;
} // namespace loc::cfg
