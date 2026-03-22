#include <IRremoteESP32.h>
#include <driver/dac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <LittleFS.h>

// Pins
const int IR_RECEIVER_PIN = 18;
const int DAC_CHANNEL = DAC_CHANNEL1;
const int AUDIO_INPUT_PIN = 34;

// Audio Configuration
const int SAMPLE_BUFFER_SIZE = 32768;
const int DEFAULT_SAMPLING_RATE_HZ = 2000;
const int SECTIONS = 8;
const int SECTION_SIZE = SAMPLE_BUFFER_SIZE / SECTIONS;

// Samsung IR Codes
const uint32_t IR_SAM_RECORD   = 0xE0E040BF;
const uint32_t IR_SAM_STOP     = 0xE0E0D02F;
const uint32_t IR_SAM_P_UP     = 0xE0E0E01F;
const uint32_t IR_SAM_P_DOWN   = 0xE0E0D02E;
const uint32_t IR_SAM_SOURCE   = 0xE0E0807F; // Save sequence
const uint32_t IR_SAM_SUBTITLE = 0xE0E0A45B; // Load sequence

const uint32_t PAD_CODES[] = {
  0xE0E020DF, 0xE0E0A05F, 0xE0E0609F, 0xE0E010EF,
  0xE0E0906F, 0xE0E050AF, 0xE0E030CF, 0xE0E0B04F
};

const uint32_t DIGIT_CODES[] = {
  0xE0E08877, 0xE0E020DF, 0xE0E0A05F, 0xE0E0609F, 0xE0E010EF,
  0xE0E0906F, 0xE0E050AF, 0xE0E030CF, 0xE0E0B04F, 0xE0E0708F
};

// State Machine
enum IRState { IDLE, WAITING_SAVE_D1, WAITING_SAVE_D2, WAITING_LOAD_D1, WAITING_LOAD_D2 };
IRState currentIRState = IDLE;
int selectedFileID = 0;

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

void saveSample(int id) {
  char path[16];
  sprintf(path, "/s%02d.bin", id);
  Serial.print("Saving to "); Serial.println(path);
  if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
    File f = LittleFS.open(path, "w");
    if (f) {
      f.write(sampleBuffer, SAMPLE_BUFFER_SIZE);
      f.close();
      Serial.println("Saved!");
    }
    xSemaphoreGive(bufferMutex);
  }
}

void loadSample(int id) {
  char path[16];
  sprintf(path, "/s%02d.bin", id);
  Serial.print("Loading from "); Serial.println(path);
  if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
    File f = LittleFS.open(path, "r");
    if (f) {
      f.read(sampleBuffer, SAMPLE_BUFFER_SIZE);
      f.close();
      Serial.println("Loaded!");
    }
    xSemaphoreGive(bufferMutex);
  }
}

void setup() {
  Serial.begin(115200);
  irrecv.enableIRAM();
  dac_output_enable(DAC_CHANNEL);
  bufferMutex = xSemaphoreCreateMutex();

  if(!LittleFS.begin(true)){
    Serial.println("LittleFS Mount Failed");
  }

  xTaskCreate(samplingTask, "SamplingTask", 4096, NULL, 15, NULL);
  xTaskCreate(playbackTask, "PlaybackTask", 4096, NULL, 15, NULL);

  Serial.println("MPC Sampler + Flash Ready");
}

int getDigit(uint32_t code) {
  for (int i=0; i<10; i++) if (code == DIGIT_CODES[i]) return i;
  return -1;
}

void loop() {
  if (irrecv.decode(&results)) {
    int digit = getDigit(results.value);

    switch(currentIRState) {
      case IDLE:
        if (results.value == IR_SAM_SOURCE) {
          currentIRState = WAITING_SAVE_D1;
          Serial.println("SAVE MODE: Digit 1?");
        } else if (results.value == IR_SAM_SUBTITLE) {
          currentIRState = WAITING_LOAD_D1;
          Serial.println("LOAD MODE: Digit 1?");
        } else if (results.value == IR_SAM_RECORD) {
          isRecording = !isRecording;
        } else if (results.value == IR_SAM_STOP) {
          activePlaybackSection = -1;
        } else if (results.value == IR_SAM_P_UP) {
          playbackSpeed += 0.05;
        } else if (results.value == IR_SAM_P_DOWN) {
          playbackSpeed -= 0.05;
        } else {
          for (int i = 0; i < SECTIONS; i++) {
            if (results.value == PAD_CODES[i]) { activePlaybackSection = i; break; }
          }
        }
        break;

      case WAITING_SAVE_D1:
        if (digit != -1) { selectedFileID = digit * 10; currentIRState = WAITING_SAVE_D2; }
        else currentIRState = IDLE;
        break;
      case WAITING_SAVE_D2:
        if (digit != -1) { selectedFileID += digit; saveSample(selectedFileID); }
        currentIRState = IDLE;
        break;

      case WAITING_LOAD_D1:
        if (digit != -1) { selectedFileID = digit * 10; currentIRState = WAITING_LOAD_D2; }
        else currentIRState = IDLE;
        break;
      case WAITING_LOAD_D2:
        if (digit != -1) { selectedFileID += digit; loadSample(selectedFileID); }
        currentIRState = IDLE;
        break;
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
