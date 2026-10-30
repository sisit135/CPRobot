// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  uint32_t id, op, key, revision, offset;
  float value;
} remote_request;
int remote_decode(const uint8_t *p, size_t n, remote_request *out);
size_t remote_status(uint8_t *p, size_t cap, uint32_t id, uint32_t status);
#ifdef __cplusplus
}
#endif
