#include "wire.h"
#include <string.h>
uint16_t axi_crc16(const uint8_t *bytes, size_t size) {
  uint16_t crc = 0xffff;
  for (size_t i = 0; i < size; ++i) {
    crc ^= (uint16_t)bytes[i] << 8;
    for (unsigned j = 0; j < 8; ++j)
      crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0));
  }
  return crc;
}
void axi_control_pack(uint8_t out[10], uint8_t magic1, uint32_t id,
                      uint8_t operation, uint8_t status) {
  if (!out)
    return;
  out[0] = AXI_SPI_MAGIC0;
  out[1] = magic1;
  for (unsigned i = 0; i < 4; ++i)
    out[2 + i] = (uint8_t)(id >> (8 * i));
  out[6] = operation;
  out[7] = status;
  const uint16_t crc = axi_crc16(out, 8);
  out[8] = (uint8_t)crc;
  out[9] = (uint8_t)(crc >> 8);
}
int axi_control_unpack(const uint8_t in[10], uint8_t magic1, uint32_t *id,
                       uint8_t *operation, uint8_t *status) {
  if (!in || !id || !operation || !status || in[0] != AXI_SPI_MAGIC0 ||
      in[1] != magic1)
    return 0;
  const uint16_t crc = axi_crc16(in, 8);
  if (in[8] != (uint8_t)crc || in[9] != (uint8_t)(crc >> 8))
    return 0;
  *id = (uint32_t)in[2] | ((uint32_t)in[3] << 8) | ((uint32_t)in[4] << 16) |
        ((uint32_t)in[5] << 24);
  *operation = in[6];
  *status = in[7];
  return 1;
}
int axi_spi_pack(const uint8_t *payload, size_t size,
                 uint8_t cell[AXI_SPI_SIZE]) {
  if (!payload || !cell || !size || size > AXI_MAX_PROTO)
    return 0;
  memset(cell, 0, AXI_SPI_SIZE);
  cell[0] = AXI_SPI_MAGIC0;
  cell[1] = AXI_SPI_MAGIC1;
  cell[2] = (uint8_t)size;
  cell[3] = (uint8_t)(size >> 8);
  memcpy(cell + 4, payload, size);
  const uint16_t crc = axi_crc16(cell, size + 4);
  cell[size + 4] = (uint8_t)crc;
  cell[size + 5] = (uint8_t)(crc >> 8);
  return 1;
}
int axi_spi_unpack(const uint8_t cell[AXI_SPI_SIZE], const uint8_t **payload,
                   size_t *size) {
  if (!cell || !payload || !size || cell[0] != AXI_SPI_MAGIC0 ||
      cell[1] != AXI_SPI_MAGIC1)
    return 0;
  const size_t len = (size_t)cell[2] | ((size_t)cell[3] << 8);
  if (!len || len > AXI_MAX_PROTO)
    return 0;
  const uint16_t crc = axi_crc16(cell, len + 4);
  if (cell[len + 4] != (uint8_t)crc || cell[len + 5] != (uint8_t)(crc >> 8))
    return 0;
  *payload = cell + 4;
  *size = len;
  return 1;
}
int axi_spi_typed_pack(uint8_t kind, const uint8_t *payload, size_t size,
                       uint8_t cell[AXI_SPI_SIZE]) {
  if ((kind != 4 && kind != 5 && kind != 7 && kind != 9 && kind != 10 &&
       kind != 11) ||
      !payload || !cell || !size || size > AXI_MAX_TYPED_PROTO)
    return 0;
  memset(cell, 0, AXI_SPI_SIZE);
  cell[0] = AXI_SPI_MAGIC0;
  cell[1] = AXI_SPI_TYPED_MAGIC1;
  cell[2] = (uint8_t)(size + 1);
  cell[3] = (uint8_t)((size + 1) >> 8);
  cell[4] = kind;
  memcpy(cell + 5, payload, size);
  const uint16_t crc = axi_crc16(cell, size + 5);
  cell[size + 5] = (uint8_t)crc;
  cell[size + 6] = (uint8_t)(crc >> 8);
  return 1;
}
int axi_spi_typed_unpack(const uint8_t cell[AXI_SPI_SIZE], uint8_t *kind,
                         const uint8_t **payload, size_t *size) {
  if (!cell || !kind || !payload || !size || cell[0] != AXI_SPI_MAGIC0 ||
      cell[1] != AXI_SPI_TYPED_MAGIC1)
    return 0;
  const size_t length = (size_t)cell[2] | ((size_t)cell[3] << 8);
  if (length < 2 || length > AXI_MAX_TYPED_PROTO + 1 ||
      (cell[4] != 4 && cell[4] != 5 && cell[4] != 7 && cell[4] != 9 &&
       cell[4] != 10 && cell[4] != 11))
    return 0;
  const uint16_t crc = axi_crc16(cell, length + 4);
  if (cell[length + 4] != (uint8_t)crc ||
      cell[length + 5] != (uint8_t)(crc >> 8))
    return 0;
  *kind = cell[4];
  *payload = cell + 5;
  *size = length - 1;
  return 1;
}
