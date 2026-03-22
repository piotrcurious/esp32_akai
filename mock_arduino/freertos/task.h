#ifndef TASK_H
#define TASK_H

#include "FreeRTOS.h"

typedef void (*TaskFunction_t)(void*);

void vTaskDelay(uint32_t ticks);
void xTaskCreate(TaskFunction_t task, const char* name, uint32_t stack, void* params, int priority, TaskHandle_t* handle);
TickType_t xTaskGetTickCount();
void vTaskDelayUntil(TickType_t* pxPreviousWakeTime, const TickType_t xTimeIncrement);

#endif
