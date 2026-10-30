#ifndef AXI_VL53L0X_PLATFORM_H
#define AXI_VL53L0X_PLATFORM_H
/* Platform replacement for STSW-IMG005. Keep the ST core and its notices.
 * Compile this port instead of ST's sample vl53l0x_platform.c/.h.
 * Only one owner may access this I2C bus; sequence hooks are not mutexes.
 */
#include "stm32f4xx_hal.h"
#include "vl53l0x_def.h"
#include "vl53l0x_platform_log.h"
#include <stdint.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  VL53L0X_DevData_t Data;
  I2C_HandleTypeDef *I2cHandle;
  uint8_t I2cDevAddr; /* ST/HAL 8-bit address: seven-bit address << 1. */
  uint8_t comms_type;
  uint16_t comms_speed_khz;
  uint32_t i2c_timeout_ms;
  int64_t
      deadline_us; /* Shared hal_time_us() domain; 0 disables outer deadline. */
} VL53L0X_Dev_t;
typedef VL53L0X_Dev_t *VL53L0X_DEV;
#define PALDevDataGet(Dev, field) ((Dev)->Data.field)
#define PALDevDataSet(Dev, field, value) ((Dev)->Data.field = (value))
#ifndef VL53L0X_COPYSTRING
#define VL53L0X_COPYSTRING(destination, source) strcpy((destination), (source))
#endif

/** @brief Validate access in a single bus-owner design; does not acquire a
 * lock.
 * @param[in] Dev Device context.
 * @returns ST error code; no ISR or task synchronization is performed.
 */
VL53L0X_Error VL53L0X_LockSequenceAccess(VL53L0X_DEV Dev);
/** @brief Complete a single-owner sequence; no mutex is held.
 * @param[in] Dev Device context.
 * @returns ST error code.
 */
VL53L0X_Error VL53L0X_UnlockSequenceAccess(VL53L0X_DEV Dev);
/** @brief Write bytes using blocking/polling HAL_I2C_Mem_Write (never IT/DMA).
 * @param[in] Dev Device context with an initialized bus and finite timeout.
 * @param[in] index First eight-bit register.
 * @param[in] data Source bytes.
 * @param[in] count Number of bytes, within the register window.
 * @returns ST error code.
 */
VL53L0X_Error VL53L0X_WriteMulti(VL53L0X_DEV Dev, uint8_t index, uint8_t *data,
                                 uint32_t count);
/** @brief Read bytes using blocking/polling HAL_I2C_Mem_Read (never IT/DMA).
 * @param[in] Dev Device context.
 * @param[in] index First register.
 * @param[out] data Destination bytes.
 * @param[in] count Destination length, within the register window.
 * @returns ST error code.
 */
VL53L0X_Error VL53L0X_ReadMulti(VL53L0X_DEV Dev, uint8_t index, uint8_t *data,
                                uint32_t count);
/** @brief Write an eight-bit register value.
 * @param[in] Dev Device context.
 * @param[in] index Register address.
 * @param[in] data Value.
 * @returns ST error code.
 */
VL53L0X_Error VL53L0X_WrByte(VL53L0X_DEV Dev, uint8_t index, uint8_t data);
/** @brief Write a big-endian 16-bit value.
 * @param[in] Dev Device context.
 * @param[in] index First register.
 * @param[in] data Value.
 * @returns ST error code.
 */
VL53L0X_Error VL53L0X_WrWord(VL53L0X_DEV Dev, uint8_t index, uint16_t data);
/** @brief Write a big-endian 32-bit value.
 * @param[in] Dev Device context.
 * @param[in] index First register.
 * @param[in] data Value.
 * @returns ST error code.
 */
VL53L0X_Error VL53L0X_WrDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t data);
/** @brief Read an eight-bit register value.
 * @param[in] Dev Device context.
 * @param[in] index Register address.
 * @param[out] data Value; unchanged on failure.
 * @returns ST error code.
 */
VL53L0X_Error VL53L0X_RdByte(VL53L0X_DEV Dev, uint8_t index, uint8_t *data);
/** @brief Read a big-endian 16-bit value.
 * @param[in] Dev Device context.
 * @param[in] index First register.
 * @param[out] data Value; unchanged on failure.
 * @returns ST error code.
 */
VL53L0X_Error VL53L0X_RdWord(VL53L0X_DEV Dev, uint8_t index, uint16_t *data);
/** @brief Read a big-endian 32-bit value.
 * @param[in] Dev Device context.
 * @param[in] index First register.
 * @param[out] data Value; unchanged on failure.
 * @returns ST error code.
 */
VL53L0X_Error VL53L0X_RdDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t *data);
/** @brief Read/modify/write a byte in the single-owner context.
 * @param[in] Dev Device context.
 * @param[in] index Register address.
 * @param[in] and_mask Mask applied before or_mask.
 * @param[in] or_mask Bits to set.
 * @returns ST error code.
 */
VL53L0X_Error VL53L0X_UpdateByte(VL53L0X_DEV Dev, uint8_t index,
                                 uint8_t and_mask, uint8_t or_mask);
/** @brief Provide a bounded one-millisecond delay for ST
 * initialization/calibration.
 * @param[in] Dev Device context with the caller's operation deadline.
 * @returns TIME_OUT after the deadline, otherwise NONE.
 * @note Runtime ranging readiness is polled across calls, not inside this
 * delay.
 */
VL53L0X_Error VL53L0X_PollingDelay(VL53L0X_DEV Dev);
#ifdef __cplusplus
}
#endif
#endif
