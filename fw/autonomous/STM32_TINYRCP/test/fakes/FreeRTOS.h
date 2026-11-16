// SPDX-License-Identifier: GPL-3.0-only

// Test declarations only; never add this include path to firmware.
#pragma once
#include <cstdint>
using TickType_t = uint32_t;
using BaseType_t = int;
using UBaseType_t = unsigned;
constexpr int pdTRUE = 1, pdFALSE = 0, pdPASS = 1;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(ms) (static_cast<TickType_t>(ms))

#define configMAX_PRIORITIES 5
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
