#include <IRremoteESP32.h>
#include <driver/dac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Define pins
const int IR_RECEIVER_PIN = 18;
const int DAC_CHANNEL = DAC_CHANNEL1;
const int AUDIO_INPUT_PIN = 34;

// Audio Configuration (MPC-style: 8 Pads, 32KB buffer)
const int SAMPLE_BUFFER_SIZE = 32768;
const int DEFAULT_SAMPLING_RATE_HZ = 2000;
const int SECTIONS = 8;
const int SECTION_SIZE = SAMPLE_BUFFER_SIZE / SECTIONS;

// Samsung DVD / TV IR Codes (32-bit HEX)
const uint32_t IR_SAM_RECORD = 0xE0E040BF;
const uint32_t IR_SAM_STOP   = 0xE0E0D02F;
const uint32_t IR_SAM_P_UP   = 0xE0E0E01F;
const uint32_t IR_SAM_P_DOWN = 0xE0E0D02E; // Fixed code to avoid collision with Stop
const uint32_t IR_SAM_PLAY   = 0xE0E010EF;

// Pad Mappings (Digits 1-8)
const uint32_t PAD_CODES[] = {
  0xE0E020DF, // 1
  0xE0E0A05F, // 2
  0xE0E0609F, // 3
  0xE0E010EF, // 4
  0xE0E0906F, // 5
  0xE0E050AF, // 6
  0xE0E030CF, // 7
  0xE0E0B04F  // 8
};

// Forward Declarations
void samplingTask(void *pvParameters);
void playbackTask(void *pvParameters);

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

  xTaskCreate(samplingTask, "SamplingTask", 4096, NULL, 15, NULL);
  xTaskCreate(playbackTask, "PlaybackTask", 4096, NULL, 15, NULL);

  Serial.println("MPC Sampler Ready - Samsung DVD Mapping");
}

void loop() {
  if (irrecv.decode(&results)) {
    Serial.print("IR Code: ");
    Serial.println(String(results.value, HEX).c_str());

    if (results.value == IR_SAM_RECORD) {
      isRecording = !isRecording;
      Serial.println(isRecording ? "REC ENABLED" : "REC DISABLED");
    } else if (results.value == IR_SAM_STOP) {
      activePlaybackSection = -1;
      Serial.println("ALL STOP");
    } else if (results.value == IR_SAM_P_UP) {
      playbackSpeed += 0.05;
      if (playbackSpeed > 3.0) playbackSpeed = 3.0;
      Serial.println("Speed UP");
    } else if (results.value == IR_SAM_P_DOWN) {
      playbackSpeed -= 0.05;
      if (playbackSpeed < 0.2) playbackSpeed = 0.2;
      Serial.println("Speed DOWN");
    } else {
      for (int i = 0; i < SECTIONS; i++) {
        if (results.value == PAD_CODES[i]) {
          activePlaybackSection = i;
          Serial.println("TRIG PAD");
          break;
        }
      }
    }
    irrecv.resume();
  }
}

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
