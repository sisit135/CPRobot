// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Exactly one sensor backend. Existing profiles retain the MPU9250 default.
#ifndef AXI_IMU_LSM303DLHC
#define AXI_IMU_LSM303DLHC 0
#endif
#ifndef AXI_IMU_MPU9250
#define AXI_IMU_MPU9250 (!AXI_IMU_LSM303DLHC)
#endif
#if (AXI_IMU_MPU9250 != 0 && AXI_IMU_MPU9250 != 1) ||                          \
    (AXI_IMU_LSM303DLHC != 0 && AXI_IMU_LSM303DLHC != 1) ||                    \
    (AXI_IMU_MPU9250 + AXI_IMU_LSM303DLHC != 1)
#error "Select exactly one IMU backend: AXI_IMU_MPU9250 or AXI_IMU_LSM303DLHC"
#endif
