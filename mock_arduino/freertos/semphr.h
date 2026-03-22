#ifndef SEMPHR_H
#define SEMPHR_H

#include "FreeRTOS.h"

SemaphoreHandle_t xSemaphoreCreateMutex();
bool xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime);
bool xSemaphoreGive(SemaphoreHandle_t xSemaphore);

#endif
