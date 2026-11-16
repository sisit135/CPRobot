// SPDX-License-Identifier: GPL-3.0-only

// Test declarations only; never add this include path to firmware.
#pragma once
#include "FreeRTOS.h"
using EventGroupHandle_t = void *;
using EventBits_t = uint32_t;
EventGroupHandle_t xEventGroupCreate();
EventBits_t xEventGroupWaitBits(EventGroupHandle_t, EventBits_t, BaseType_t,
                                BaseType_t, TickType_t);
EventBits_t xEventGroupSetBits(EventGroupHandle_t, EventBits_t);
void vEventGroupDelete(EventGroupHandle_t);
