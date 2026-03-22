#include "Arduino.h"
#include "IRremoteESP32.h"
#include "driver/dac.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <vector>

SerialMock Serial;

void delay(uint32_t ms) {}
uint32_t millis() { return 0; }
int analogRead(int pin) { return 2048; } // Mock value

IrReceiverMock IrReceiver;

void set_mock_ir_values(const std::vector<uint32_t>& values) {
    IrReceiver.set_mock_values(values);
}

esp_err_t dac_output_enable(int channel) { return 0; }
esp_err_t dac_output_voltage(int channel, int voltage) { return 0; }
void dacWrite(int channel, int value) {}

void vTaskDelay(uint32_t ticks) {}
static std::vector<TaskFunction_t> tasks;
void xTaskCreate(TaskFunction_t task, const char* name, uint32_t stack, void* params, int priority, TaskHandle_t* handle) {
    tasks.push_back(task);
}

void run_mock_tasks(int iterations) {
    for (int i = 0; i < iterations; i++) {
        for (auto task : tasks) {
            task(NULL);
        }
    }
}

TickType_t xTaskGetTickCount() { return 0; }
void vTaskDelayUntil(TickType_t* pxPreviousWakeTime, const TickType_t xTimeIncrement) {}

SemaphoreHandle_t xSemaphoreCreateMutex() { return (SemaphoreHandle_t)1; }
bool xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime) { return true; }
bool xSemaphoreGive(SemaphoreHandle_t xSemaphore) { return true; }
