#ifndef FREERTOS_H
#define FREERTOS_H

#include <stdint.h>

typedef void* TaskHandle_t;
typedef void* SemaphoreHandle_t;
typedef uint32_t TickType_t;

#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY (TickType_t)0xffffffffUL
#define pdMS_TO_TICKS(ms) (ms)

#endif
