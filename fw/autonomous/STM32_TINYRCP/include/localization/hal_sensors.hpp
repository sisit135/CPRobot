// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "localization/color_tracker.hpp"
#include "localization/types.hpp"
#include "navigation/end_cell.hpp"
// Classify a fresh HAL integration as a debounced white-to-black end tape
// entry.
bool hal_end_tape_classify(const loc::RgbcSample &, int64_t timestamp_us,
                           nav::EndTapeEvent *);
// All sensor bus access belongs to the sensor task. Board reads must be
// bounded. Timestamps share hal_time_us(); zero requests a read-completion
// timestamp.
/** @brief Start both sensor buses, board sensors and any enabled color driver.
 * @returns true when initialization succeeds; false on an I2C/identity failure.
 */
bool hal_sensors_init();
// BUP calls this before motor/settings/device HAL initialization. Idempotent:
// probes GPIO first, then prepares and inventories both hardware controllers.
bool hal_board_sensor_buses_init();
// Startup-owner primitives for independent bench initialization. No inventory
// or other device initialization is performed by these functions.
bool hal_board_main_bus_init(bool configure_clock = true);
bool hal_board_tof_bus_init();
bool hal_board_imu_init();
bool hal_board_tof_init();
// Disabled by default through hal/color_selection.hpp: these hooks then return
// false without bus access. Enabled driver fixtures require main-bus setup.
bool hal_tcs34725_init();
// True while the TCS driver is initialized, including power/integration waits.
// No bus access; use only from the startup/sensor owner.
bool hal_tcs34725_initialized();
/** @brief Initialize board-specific IMU and ToF devices after Wire.begin().
 * @returns true when board sensor initialization succeeds.
 */
bool hal_board_sensors_init();
/** @brief Read a fresh body acceleration and planar yaw-rate frame.
 * @param[out] out Non-null destination; gz is rad/s (MPU gyro or the selected
 * LSM303DLHC MAG+ACCEL compass derivative); acceleration is diagnostic to EKF.
 * @param[out] timestamp_us Acquisition time, or zero for read completion.
 * @returns true for a new valid body frame; false while not ready or faulted.
 */
bool hal_imu_read(loc::ImuSample *out, int64_t *timestamp_us);
/** @brief Read a fresh calibrated, tilt-compensated magnetic field.
 * @param[out] out Non-null body field in microteslas.
 * @param[out] timestamp_us Acquisition time, or zero for read completion.
 * @returns true only for new calibrated, gravity-levelled data; false before
 * calibration.
 */
bool hal_mag_read(loc::MagSample *out, int64_t *timestamp_us);
/** @brief Check that the board has qualified the magnetic field for heading.
 * @returns true only when a magnetic measurement is usable; default false.
 * @note A zero command alone does not prove magnetic cleanliness or rest.
 * The gyro-free LSM backend requires a separately verified magnetic environment
 * during motion; otherwise it publishes no navigation measurements.
 */
bool hal_magnetic_clean();
/** @brief Poll one corrected physical-wall range.
 * @param[in] id LEFT, FRONT, or RIGHT sensor.
 * @param[out] distance_m Non-null destination in meters.
 * @param[out] timestamp_us Acquisition time, or zero for read completion.
 * @returns true for fresh valid data; default false. Spelling is preserved.
 */
bool hal_tof_red_distance_corrected(loc::TofId id, float *distance_m,
                                  int64_t *timestamp_us);
/** @brief Legacy compatibility hook: returns no references on the all-white
 * field.
 * @param[out] count Number of static references; non-null.
 * @returns Array valid for the run; default nullptr with count=0.
 * @note Repeated indistinguishable colors require a planner association layer.
 */
const loc::ColorReference *hal_color_references(std::size_t *count);
/** @brief Advance the TCS34725 driver without waiting for integration.
 * @param[out] out Non-null debounced mapped event.
 * @param[out] timestamp_us Read-completion time in the shared clock domain.
 * @returns true on a new mapped entry event, false while
 * waiting/ambiguous/faulted.
 */
bool hal_color_marker_read(loc::MarkerSample *out, int64_t *timestamp_us);

/** @brief Poll one fresh completed optical integration without map association.
 * @param[out] out Non-null raw RGBC destination.
 * @param[out] timestamp_us Read-completion timestamp in the shared clock
 * domain.
 * @returns true for a new frame, false while waiting or on a bus fault.
 */
bool hal_tcs34725_read(loc::RgbcSample *out, int64_t *timestamp_us);
/** @brief Legacy compatibility hook: always false on the all-white field.
 * @param[in] raw Fresh integration with a unique sequence number.
 * @param[out] out Non-null debounced marker event.
 * @returns false; use hal_end_tape_classify for the black entry tape.
 */
// Legacy color-to-cell classification is disabled on the all-white field.
bool hal_color_classify(const loc::RgbcSample &raw, loc::MarkerSample *out);
