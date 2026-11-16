// SPDX-License-Identifier: GPL-3.0-only

// Test declarations only; never add this include path to firmware.
#pragma once
#include "FreeRTOS.h"
using QueueHandle_t = void *;
QueueHandle_t xQueueCreate(UBaseType_t, UBaseType_t);
BaseType_t xQueueSend(QueueHandle_t, const void *, TickType_t);
BaseType_t xQueueReceive(QueueHandle_t, void *, TickType_t);
BaseType_t xQueuePeek(QueueHandle_t, void *, TickType_t);
BaseType_t xQueueOverwrite(QueueHandle_t, const void *);
void vQueueDelete(QueueHandle_t);
