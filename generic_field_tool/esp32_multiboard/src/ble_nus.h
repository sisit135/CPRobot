// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include <stddef.h>
#include <stdint.h>
/** @brief Initialize a Nordic UART Service peripheral for provisioning and fallback.
 * @returns 0 on success, nonzero on failure.
 */
int ble_nus_start(void);
/** @brief Send one serialized protobuf over BLE notifications when subscribed.
 * @param[in] proto Complete protobuf message.
 * @param[in] len Protobuf byte count.
 * @returns 0 if all chunks were sent, nonzero if unavailable or interrupted.
 */
int ble_nus_publish(uint8_t kind, const uint8_t *proto, size_t len);
/** @brief Queue a remote start/halt protobuf for the next SPI master poll.
 * @param[in] proto Serialized Control protobuf.
 * @param[in] len Protobuf byte count.
 * @returns Zero if accepted for delivery, nonzero otherwise.
 */
int telemetry_control_submit(const uint8_t *proto, size_t len);
/** @brief Queue a color-reference calibration request for the STM32 sensor task.
 * @param[in] proto Serialized axi.remote.v1.CalibrateColorRequest.
 * @param[in] len Protobuf byte count.
 * @returns Zero if accepted for delivery, nonzero otherwise.
 */
int telemetry_color_calibration_submit(const uint8_t *proto,size_t len);
/** @brief Apply a length-prefixed Wi-Fi credential command from an encrypted GATT write.
 * @param[in] ssid SSID bytes, not necessarily zero terminated.
 * @param[in] ssid_len SSID byte count.
 * @param[in] pass Password bytes.
 * @param[in] pass_len Password byte count.
 * @returns 0 if accepted by Wi-Fi driver, nonzero on invalid input/error.
 */
int telemetry_wifi_provision(const uint8_t *ssid, size_t ssid_len, const uint8_t *pass, size_t pass_len);

int telemetry_remote_submit(const uint8_t *proto,size_t len);

void ble_nus_pairing_window(unsigned seconds);
int telemetry_link_info(uint32_t request_id);

int telemetry_generic_submit(const uint8_t *proto, size_t len);
