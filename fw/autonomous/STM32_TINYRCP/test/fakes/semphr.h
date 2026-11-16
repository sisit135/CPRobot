// SPDX-License-Identifier: GPL-3.0-only

#pragma once
#include "FreeRTOS.h"
using SemaphoreHandle_t = void *;
SemaphoreHandle_t xSemaphoreCreateMutex();
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t);
BaseType_t xSemaphoreGive(SemaphoreHandle_t);
void vSemaphoreDelete(SemaphoreHandle_t);
