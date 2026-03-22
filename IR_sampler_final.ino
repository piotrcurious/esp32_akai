#include <IRremoteESP32.h>
#include <driver/dac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Define pins
const int IR_RECEIVER_PIN = 18;
const int DAC_CHANNEL = DAC_CHANNEL1;
const int AUDIO_INPUT_PIN = 34;

// Audio Configuration
const int SAMPLE_BUFFER_SIZE = 8192;
const int DEFAULT_SAMPLING_RATE_HZ = 1000;
const int SECTIONS = 8;
const int SECTION_SIZE = SAMPLE_BUFFER_SIZE / SECTIONS;

// IR Codes
const uint32_t IR_RECORD = 0xFFA25D;

// IR library
IRrecv irrecv(IR_RECEIVER_PIN);
decode_results results;

// Buffer and State
uint8_t sampleBuffer[SAMPLE_BUFFER_SIZE];
volatile bool isRecording = false;
volatile int activePlaybackSection = -1;
volatile float playbackSpeed = 1.0;

SemaphoreHandle_t bufferMutex;

void setup() {
  Serial.begin(115200);
  irrecv.enableIRAM();
  dac_output_enable(DAC_CHANNEL);
  bufferMutex = xSemaphoreCreateMutex();
}

void loop() {
  if (irrecv.decode(&results)) {
    if (results.value == IR_RECORD) {
      isRecording = !isRecording;
    } else {
      uint32_t playCodes[] = {0xFF30CF, 0xFF18E7, 0xFF7A85, 0xFF10EF, 0xFF38C7, 0xFF5AA5, 0xFF42BD, 0xFF4AB5};
      for (int i = 0; i < SECTIONS; i++) {
        if (results.value == playCodes[i]) {
          activePlaybackSection = i;
          break;
        }
      }
    }
    irrecv.resume();
  }
}

// Actual tasks for ESP32
#ifndef TESTING_MOCK
void samplingTask(void *pvParameters) {
  int writeIndex = 0;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (true) {
    if (isRecording) {
      if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
        sampleBuffer[writeIndex] = (uint8_t)(analogRead(AUDIO_INPUT_PIN) >> 4);
        xSemaphoreGive(bufferMutex);
      }
      writeIndex = (writeIndex + 1) % SAMPLE_BUFFER_SIZE;
      vTaskDelayUntil(&xLastWakeTime, 1);
    } else {
      writeIndex = 0;
      vTaskDelay(pdMS_TO_TICKS(10));
      xLastWakeTime = xTaskGetTickCount();
    }
  }
}

void playbackTask(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (true) {
    if (activePlaybackSection != -1) {
      int section = activePlaybackSection;
      int start = section * SECTION_SIZE;
      int end = start + SECTION_SIZE;
      for (float i = start; i < end; ) {
        if (activePlaybackSection != section) break;
        int idx = (int)i;
        if (idx >= SAMPLE_BUFFER_SIZE) break;
        if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
          dacWrite(DAC_CHANNEL, sampleBuffer[idx]);
          xSemaphoreGive(bufferMutex);
        }
        i += playbackSpeed;
        vTaskDelayUntil(&xLastWakeTime, 1);
      }
      if (activePlaybackSection == section) activePlaybackSection = -1;
    } else {
      vTaskDelay(pdMS_TO_TICKS(10));
      xLastWakeTime = xTaskGetTickCount();
    }
  }
}
#endif
