// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <Arduino.h>
#include <cstdint>

// Board-level sensor wiring. All addresses here are SEVEN-bit addresses.
// THS v2 Black Pill assignment; see docs/hardware_wiring.md.
// Override macros with build flags or edit this header before deployment.
#ifndef SENSORS_MAIN_SDA_PIN
#define SENSORS_MAIN_SDA_PIN PB7
#endif
#ifndef SENSORS_MAIN_SCL_PIN
#define SENSORS_MAIN_SCL_PIN PB6
#endif
#ifndef SENSORS_MPU9250_ADDRESS_7BIT
#define SENSORS_MPU9250_ADDRESS_7BIT 0x68
#endif
// I2C2: PB3 AF9 SDA / PB10 AF4 SCL (PB3 cannot also provide SWO).
#ifndef SENSORS_TOF_SDA_PIN
#define SENSORS_TOF_SDA_PIN PB3
#endif
#ifndef SENSORS_TOF_SCL_PIN
#define SENSORS_TOF_SCL_PIN PB10
#endif
#ifndef SENSORS_TOF_LEFT_XSHUT_PIN
#define SENSORS_TOF_LEFT_XSHUT_PIN PA1
#endif
#ifndef SENSORS_TOF_FRONT_XSHUT_PIN
#define SENSORS_TOF_FRONT_XSHUT_PIN PA2
#endif
#ifndef SENSORS_TOF_RIGHT_XSHUT_PIN
#define SENSORS_TOF_RIGHT_XSHUT_PIN PA3
#endif

#ifndef SENSORS_TAPE_BLACK_MAX_CLEAR
#define SENSORS_TAPE_BLACK_MAX_CLEAR 0
#endif
#ifndef SENSORS_TAPE_WHITE_MIN_CLEAR
#define SENSORS_TAPE_WHITE_MIN_CLEAR 0
#endif
namespace board {
namespace sensors {
// Measure at gain 4x / 24 ms integration and installed TCS height. Zero
// disables.
constexpr uint16_t kTapeBlackMaxClear = SENSORS_TAPE_BLACK_MAX_CLEAR;
constexpr uint16_t kTapeWhiteMinClear = SENSORS_TAPE_WHITE_MIN_CLEAR;
constexpr uint32_t kMainSdaPin = SENSORS_MAIN_SDA_PIN;
constexpr uint32_t kMainSclPin = SENSORS_MAIN_SCL_PIN;
constexpr uint32_t kMainI2cHz = 100000;
constexpr uint32_t kI2cTimeoutMs = 5;
constexpr uint8_t kMpu9250Address7bit = SENSORS_MPU9250_ADDRESS_7BIT;
constexpr uint8_t kAk8963Address7bit = 0x0C; // Fixed by silicon, bypass mode.
constexpr uint8_t kTcs34725Address7bit = 0x29;
constexpr uint8_t kVl53l0xBootAddress7bit = 0x29;
constexpr uint8_t kTofAddress7bit[3] = {0x30, 0x31,
                                        0x32}; // LEFT, FRONT, RIGHT.
// ToF MUST use a separate controller or external isolation: its boot address
// collides with the TCS34725, even though its assigned runtime addresses
// differ.
constexpr int kTofSdaPin = SENSORS_TOF_SDA_PIN;
constexpr int kTofSclPin = SENSORS_TOF_SCL_PIN;
constexpr int kTofXshutPin[3] = {SENSORS_TOF_LEFT_XSHUT_PIN,
                                 SENSORS_TOF_FRONT_XSHUT_PIN,
                                 SENSORS_TOF_RIGHT_XSHUT_PIN};
constexpr int kMpuIntPin = -1; // Unused: polling, no attachInterrupt()/EXTI.
constexpr int kTcsIntPin = -1;
constexpr int kTofGpio1Pin = -1;
// Strap MPU CS high for I2C. Strap AD0 low for 0x68, high for 0x69.
// No GPIO is invented for these straps.
constexpr int kMpuCsPin = -1;
constexpr int kMpuAd0Pin = -1;

// Signed, one-based source axes for body [+X forward, +Y left, +Z up].
// Identity values are placeholders for mounting, NOT an orientation detection.
// Configure both independently: AK8963 native axes differ from MPU accel/gyro.
constexpr int8_t kImuBodyAxes[3] = {1, 2, 3};
constexpr int8_t kMagBodyAxes[3] = {1, 2, 3};
// Accel/gyro biases are in body axes. Do not subtract gravity here.
constexpr float kAccelBiasMps2[3] = {0, 0, 0};
constexpr float kGyroBiasRps[3] = {0, 0, 0};
// Magnetic calibration is in native AK8963 axes, AFTER factory ASA adjustment.
// body = axis_map * soft_iron * (native_uT - hard_iron_uT).
constexpr float kMagHardIronUt[3] = {0, 0, 0};
constexpr float kMagSoftIron[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
constexpr bool kMagCalibrationAndAxesVerified = false;
constexpr uint16_t kImuSampleRateHz = 100; // Fixed 1 kHz filtered source / 10.
constexpr uint32_t kMagTiltMaxAgeUs = 30000;
constexpr float kMagFieldMinUt = 15.0f;
constexpr float kMagFieldMaxUt = 100.0f;
constexpr float kTiltGravityToleranceMps2 = 1.0f;
constexpr float kTiltMaxGyroRps = 0.15f;
constexpr uint32_t kTiltSettleUs = 250000;
static_assert(kMpu9250Address7bit == 0x68 || kMpu9250Address7bit == 0x69,
              "MPU9250 supports I2C address 0x68 or 0x69 only");
static_assert(kAk8963Address7bit != kTcs34725Address7bit, "Address collision");
} // namespace sensors
} // namespace board
