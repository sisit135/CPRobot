// Test declarations only; never add this include path to firmware.
#pragma once
#include "FreeRTOS.h"
using TaskHandle_t = void *;
using TaskFunction_t = void (*)(void *);
BaseType_t xTaskCreate(TaskFunction_t, const char *, uint32_t, void *,
                       UBaseType_t, TaskHandle_t *);
TickType_t xTaskGetTickCount();
void vTaskDelayUntil(TickType_t *, TickType_t);
void vTaskDelete(TaskHandle_t);

void vTaskStartScheduler();

void vTaskDelay(TickType_t);
