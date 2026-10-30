#ifndef AXI_TELEMETRY_WIRE_H
#define AXI_TELEMETRY_WIRE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AXI_SPI_SIZE 256u
#define AXI_MAX_PROTO 240u
#define AXI_SPI_MAGIC0 0xA6u
#define AXI_SPI_MAGIC1 0x51u
#define AXI_SPI_CMD_MAGIC1 0x52u
#define AXI_SPI_ACK_MAGIC1 0x53u
#define AXI_SPI_TYPED_MAGIC1 0x54u
#define AXI_MAX_TYPED_PROTO 238u
/* Stream frame: uint16 LE size, uint8 kind (1=EkfLog, 2=Control,
 * 3=ControlAck, 4/5=remote config request/reply, 7=LogRecord,
 * 8=CalibrateColorRequest, 9/10=generic command/response, 11=sensor envelope),
 * protobuf. */
/* SPI command/ack occupy the first 10 bytes (magic[2], id LE32, op, status, crc
 * LE16). */
/** @brief Pack a command or acknowledgment in the 10-byte SPI header.
 * @param[out] out Ten writable bytes.
 * @param[in] magic1 AXI_SPI_CMD_MAGIC1 or AXI_SPI_ACK_MAGIC1.
 * @param[in] id Request identifier.
 * @param[in] operation Control operation.
 * @param[in] status Acknowledgment result, or color slot (0..3) for op 3.
 * @returns No value.
 */
void axi_control_pack(uint8_t out[10], uint8_t magic1, uint32_t id,
                      uint8_t operation, uint8_t status);
/** @brief Validate one SPI control header.
 * @param[in] in Ten readable bytes.
 * @param[in] magic1 Expected command/ack discriminator.
 * @param[out] id Request identifier.
 * @param[out] operation Operation byte.
 * @param[out] status Status byte.
 * @returns 1 for a valid header, 0 otherwise.
 */
int axi_control_unpack(const uint8_t in[10], uint8_t magic1, uint32_t *id,
                       uint8_t *operation, uint8_t *status);
/* SPI: magic[2], little-endian length[2], protobuf[len], CRC16-CCITT[2], zero
 * padding. */
/** @brief Compute CRC-16/CCITT-FALSE with initial value 0xffff.
 * @param[in] bytes Buffer to checksum.
 * @param[in] size Number of bytes.
 * @returns CRC-16.
 */
uint16_t axi_crc16(const uint8_t *bytes, size_t size);
/** @brief Pack a complete serialized protobuf into one SPI transaction.
 * @param[in] payload Serialized protobuf bytes.
 * @param[in] size Payload length in bytes (1..240).
 * @param[out] cell Exactly 256 writable bytes.
 * @returns 1 on success, 0 on invalid input.
 */
int axi_spi_pack(const uint8_t *payload, size_t size,
                 uint8_t cell[AXI_SPI_SIZE]);
/** @brief Validate a SPI transaction and locate its protobuf bytes.
 * @param[in] cell Exactly 256 received bytes.
 * @param[out] payload View inside cell, valid while cell remains alive.
 * @param[out] size Decoded byte count.
 * @returns 1 for a valid complete frame, 0 otherwise.
 */
int axi_spi_unpack(const uint8_t cell[AXI_SPI_SIZE], const uint8_t **payload,
                   size_t *size);
/** Pack a typed protobuf frame (kind is forwarded unchanged by the RA bridge).
 */
int axi_spi_typed_pack(uint8_t kind, const uint8_t *payload, size_t size,
                       uint8_t cell[AXI_SPI_SIZE]);
/** Unpack a typed protobuf frame. */
int axi_spi_typed_unpack(const uint8_t cell[AXI_SPI_SIZE], uint8_t *kind,
                         const uint8_t **payload, size_t *size);
#ifdef __cplusplus
}
#endif
#endif
