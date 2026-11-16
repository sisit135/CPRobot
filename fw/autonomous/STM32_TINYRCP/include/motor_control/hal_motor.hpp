// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "motor_control/types.hpp"

/** @brief Initialize TI DRV8833 inputs at zero PWM and wake the bridge.
 * @returns true on valid configuration and deasserted optional nFAULT.
 * @note Also called during HAL startup before tasks exist. Four xIN pins must
 *       support PWM; nSLEEP must be controlled or explicitly tied high. A
 * bounded 1 ms wake delay runs only at initialization, not each control update.
 */
bool hal_motor_init();
/** @brief Apply signed normalized duties using DRV8833 fast-decay PWM.
 * @param[in] left Left duty in [-1,1], positive is forward.
 * @param[in] right Right duty in [-1,1], positive is forward.
 * @returns true if requests were issued; false on invalid input, uninitialized
 *          hardware or asserted optional nFAULT. Faults disable both wheels.
 * @note Forward=(PWM,0), reverse=(0,PWM), zero=(0,0), no separate EN pins.
 *       nFAULT is polled, not a speed/stall/current measurement. When
 * connected, use a pull-up to 3.3 V. Task context only after startup.
 */
bool hal_motor_set_duty(float left, float right);
/** @brief Set all four DRV8833 inputs to zero PWM for coast.
 * @returns No value; attempts stop on both bridges.
 * @note Does not apply active braking or guarantee wheel standstill. Fault
 *       shutdown also lowers nSLEEP when controlled. Not an ISR API.
 */
void hal_motor_stop();
