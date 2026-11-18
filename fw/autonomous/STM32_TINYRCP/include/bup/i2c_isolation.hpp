// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>

#ifndef AXI_BUP_I2C_ISOLATION
#define AXI_BUP_I2C_ISOLATION 0
#endif
#if AXI_BUP_I2C_ISOLATION && (!AXI_BUP_IMU_DIAGNOSTIC || AXI_OPENLOOP_DEMO)
#error "I2C isolation requires diagnostic bring-up and excludes the motion demo"
#endif

#if AXI_BUP_I2C_ISOLATION
// Set while halted at main(), after C/C++ initialization, before each run.
// A reset restores case 1 (TCS alone). No flash/settings writes select a case.
extern "C" volatile uint32_t ths_i2c_isolation_case;
namespace bup {
void run_i2c_isolation();
}
#endif
