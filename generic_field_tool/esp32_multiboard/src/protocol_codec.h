// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int tool_control_decode(const uint8_t *, size_t, uint32_t *, uint32_t *);
int tool_color_decode(const uint8_t *, size_t, uint32_t *, uint32_t *);
int tool_envelope_valid(const uint8_t *, size_t);
uint32_t tool_reply_id(const uint8_t *, size_t);
size_t tool_ack_encode(uint8_t *, size_t, uint32_t, uint32_t, uint32_t);
size_t tool_link_encode(uint8_t *, size_t, uint32_t, const char *, const char *, uint32_t, const char *);
#ifdef __cplusplus
}
#endif
