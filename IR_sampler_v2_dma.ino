#include <Arduino.h>
#include <IRremoteESP32.h>
#include <LittleFS.h>
#ifndef TESTING_MOCK
#include <driver/dac_continuous.h>
#include <esp_adc/adc_continuous.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

/*
 * MPC-style Sampler for ESP32 using DMA for ADC and DAC
 * Optimized for Arduino ESP32 Core 3.0+ (based on ESP-IDF 5.1+)
 * Recording: ADC1 Channel 6 (GPIO 34) via Continuous ADC Driver (DMA)
 * Playback: Internal DAC Channel 1 (GPIO 25) via Continuous DAC Driver (DMA)
 */

// --- Hardware Pins ---
const int IR_RECEIVER_PIN = 18;
const adc_channel_t ADC_CHAN = ADC_CHANNEL_6; // GPIO 34
const dac_channel_mask_t DAC_CHAN_MASK = DAC_CHANNEL_MASK_CH0; // GPIO 25 (DAC1)

// --- Audio Config ---
const int SAMPLE_BUFFER_SIZE = 32768; // 32KB buffer
const int SAMPLING_RATE_HZ = 16000;   // 16kHz sample rate
const int SECTIONS = 8;
const int SECTION_SIZE = SAMPLE_BUFFER_SIZE / SECTIONS;
const int DMA_CHUNK_SIZE = 512;       // DMA transfer chunk size (bytes)

// --- Samsung IR Codes ---
const uint32_t IR_SAM_RECORD   = 0xE0E040BF;
const uint32_t IR_SAM_STOP     = 0xE0E0D02F;
const uint32_t IR_SAM_P_UP     = 0xE0E0E01F;
const uint32_t IR_SAM_P_DOWN   = 0xE0E0D02E;
const uint32_t IR_SAM_SOURCE   = 0xE0E0807F;
const uint32_t IR_SAM_SUBTITLE = 0xE0E0A45B;

const uint32_t COLOR_CODES[] = { 0xE0E036C9, 0xE0E028D7, 0xE0E0A857, 0xE0E06897 };
const uint32_t PAD_CODES[] = {
  0xE0E020DF, 0xE0E0A05F, 0xE0E0609F, 0xE0E010EF,
  0xE0E0906F, 0xE0E050AF, 0xE0E030CF, 0xE0E0B04F
};
const uint32_t DIGIT_CODES[] = {
  0xE0E08877, 0xE0E020DF, 0xE0E0A05F, 0xE0E0609F, 0xE0E010EF,
  0xE0E0906F, 0xE0E050AF, 0xE0E030CF, 0xE0E0B04F, 0xE0E0708F
};

// --- State Variables ---
uint8_t sampleBuffer[SAMPLE_BUFFER_SIZE];
volatile bool isRecording = false;
volatile int activePlaybackSection = -1;
float playbackSpeed = 1.0;
float pitchPresets[4] = { 1.0, 1.0, 1.0, 1.0 };

SemaphoreHandle_t bufferMutex;
dac_continuous_handle_t dac_handle = NULL;
adc_continuous_handle_t adc_handle = NULL;

enum IRState { IDLE, WAITING_SAVE_D1, WAITING_SAVE_D2, WAITING_LOAD_D1, WAITING_LOAD_D2 };
IRState currentIRState = IDLE;
int selectedFileID = 0;

// --- ADC DMA Initialization ---
void initADC() {
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 4096,
        .conv_frame_size = DMA_CHUNK_SIZE,
    };
    adc_continuous_new_handle(&adc_config, &adc_handle);

    adc_digi_pattern_config_t pattern = {
        .atten = ADC_ATTEN_DB_11,
        .channel = ADC_CHAN & 0x7, // Only the channel number
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12
    };

    adc_continuous_config_t dig_cfg = {
        .pattern_num = 1,
        .adc_pattern = &pattern,
        .sample_freq_hz = SAMPLING_RATE_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };
    adc_continuous_config(adc_handle, &dig_cfg);
}

// --- DAC DMA Initialization ---
void initDAC() {
    dac_continuous_config_t dac_config = {
        .chan_mask = DAC_CHAN_MASK,
        .desc_num = 8,
        .desc_size = DMA_CHUNK_SIZE,
        .sample_freq_hz = SAMPLING_RATE_HZ,
        .offset = 0,
        .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };
    dac_continuous_new_channels(&dac_config, &dac_handle);
    dac_continuous_enable(dac_handle);
    dac_continuous_start(dac_handle);
}

// --- File Operations ---
void saveSample(int id) {
  char path[16]; sprintf(path, "/s%02d.bin", id);
  if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
    File f = LittleFS.open(path, "w");
    if (f) {
      size_t written_audio = f.write(sampleBuffer, SAMPLE_BUFFER_SIZE);
      size_t written_presets = f.write((uint8_t*)pitchPresets, sizeof(pitchPresets));
      f.close();
      if (written_audio == SAMPLE_BUFFER_SIZE && written_presets == sizeof(pitchPresets)) {
          Serial.printf("Saved %s\n", path);
      } else {
          Serial.printf("Error: Partial write to %s\n", path);
      }
    } else {
        Serial.printf("Error: Failed to open %s for writing\n", path);
    }
    xSemaphoreGive(bufferMutex);
  }
}

void loadSample(int id) {
  char path[16]; sprintf(path, "/s%02d.bin", id);
  if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
    File f = LittleFS.open(path, "r");
    if (f) {
      if (f.size() < (SAMPLE_BUFFER_SIZE + sizeof(pitchPresets))) {
          Serial.printf("Error: File %s is too small\n", path);
          f.close();
          xSemaphoreGive(bufferMutex);
          return;
      }
      size_t read_audio = f.read(sampleBuffer, SAMPLE_BUFFER_SIZE);
      size_t read_presets = f.read((uint8_t*)pitchPresets, sizeof(pitchPresets));
      f.close();
      if (read_audio == SAMPLE_BUFFER_SIZE && read_presets == sizeof(pitchPresets)) {
          // Basic validation for presets
          for (int i=0; i<4; i++) {
              if (std::isnan(pitchPresets[i]) || pitchPresets[i] < 0.1 || pitchPresets[i] > 4.0) {
                  pitchPresets[i] = 1.0;
              }
          }
          Serial.printf("Loaded %s\n", path);
      } else {
          Serial.printf("Error: Partial read from %s\n", path);
      }
    } else {
        Serial.printf("Error: File %s not found\n", path);
    }
    xSemaphoreGive(bufferMutex);
  }
}

// --- Background Tasks ---

// Task to read recorded data from ADC DMA buffer
void samplingTask(void *pvParameters) {
    uint8_t result_buf[DMA_CHUNK_SIZE];
    uint32_t ret_num = 0;
    int writeIndex = 0;

    while (true) {
        if (isRecording) {
            // Read whatever is available in the DMA buffer
            esp_err_t err = adc_continuous_read(adc_handle, result_buf, DMA_CHUNK_SIZE, &ret_num, 0);
            if (err == ESP_OK && ret_num > 0) {
                if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
                    for (uint32_t i = 0; i < ret_num; i += 2) { // 2 bytes per sample in Type 1
                        adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result_buf[i];
                        // Convert 12-bit ADC (unsigned) to 8-bit for our buffer
                        // Center it around 128 (8-bit mid-range) if ADC is centered around 2048
                        int val = p->type1.data;
                        val = (val >> 4); // 0-4095 -> 0-255
                        sampleBuffer[writeIndex] = (uint8_t)val;
                        writeIndex = (writeIndex + 1) % SAMPLE_BUFFER_SIZE;
                    }
                    xSemaphoreGive(bufferMutex);
                }
            } else if (err == ESP_ERR_TIMEOUT) {
                // No data yet, just wait
            } else {
                Serial.printf("ADC Error: 0x%x\n", err);
            }
            vTaskDelay(1);
        } else {
            writeIndex = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Task to feed playback data to DAC DMA buffer
void playbackTask(void *pvParameters) {
    uint8_t dma_write_buf[DMA_CHUNK_SIZE];
    float playIndex = 0;

    while (true) {
        int section = -1;
        if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
            section = activePlaybackSection;
            xSemaphoreGive(bufferMutex);
        }

        if (section != -1) {
            int start = section * SECTION_SIZE;
            int end = start + SECTION_SIZE;
            playIndex = start;

            while (true) {
                int to_fill = 0;
                float currentSpeed = 1.0;

                if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
                    // Check if playback was stopped or changed by another task
                    if (activePlaybackSection != section) {
                        xSemaphoreGive(bufferMutex);
                        break;
                    }
                    currentSpeed = playbackSpeed;
                    // Resample from main buffer to DMA chunk with linear interpolation
                    for (int i = 0; i < DMA_CHUNK_SIZE; i++) {
                        if (playIndex >= (float)end) break;

                        int idx1 = (int)playIndex;
                        int idx2 = (idx1 + 1);
                        if (idx2 >= end) idx2 = idx1; // Boundary check

                        float frac = playIndex - (float)idx1;
                        uint8_t s1 = sampleBuffer[idx1 % SAMPLE_BUFFER_SIZE];
                        uint8_t s2 = sampleBuffer[idx2 % SAMPLE_BUFFER_SIZE];

                        dma_write_buf[i] = (uint8_t)((1.0f - frac) * s1 + frac * s2);

                        playIndex += currentSpeed;
                        to_fill++;
                    }
                    xSemaphoreGive(bufferMutex);
                }

                if (to_fill > 0) {
                    size_t written = 0;
                    // Blocks until there is space in DMA queue
                    dac_continuous_write(dac_handle, dma_write_buf, to_fill, &written, portMAX_DELAY);
                }

                if (playIndex >= (float)end) break;
            }

            // Set section to idle only if it wasn't changed to another section or stop (-1)
            if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
                if (activePlaybackSection == section) activePlaybackSection = -1;
                xSemaphoreGive(bufferMutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- IR Helper Functions ---
int getDigit(uint32_t code) { for (int i=0; i<10; i++) if (code == DIGIT_CODES[i]) return i; return -1; }
int getColor(uint32_t code) { for (int i=0; i<4; i++) if (code == COLOR_CODES[i]) return i; return -1; }

void setup() {
    Serial.begin(115200);
    IrReceiver.begin(IR_RECEIVER_PIN, ENABLE_LED_FEEDBACK);
    bufferMutex = xSemaphoreCreateMutex();
    if(!LittleFS.begin(true)){ Serial.println("LittleFS Mount Failed"); }

    // Initialize DMA Drivers
    initADC();
    initDAC();

    // Create tasks for background I/O
    xTaskCreate(samplingTask, "RecTask", 4096, NULL, 15, NULL);
    xTaskCreate(playbackTask, "PlayTask", 4096, NULL, 15, NULL);

    Serial.println("DMA Sampler (Arduino 3.0+) Ready");
}

void loop() {
  if (IrReceiver.decode()) {
    uint32_t irValue = IrReceiver.decodedIRData.decodedRawData;
    int digit = getDigit(irValue);
    int color = getColor(irValue);

    switch(currentIRState) {
      case IDLE:
        if (irValue == IR_SAM_SOURCE) { currentIRState = WAITING_SAVE_D1; Serial.println("Save mode: Select Digit (00-99) or Color (Preset)"); }
        else if (irValue == IR_SAM_SUBTITLE) { currentIRState = WAITING_LOAD_D1; Serial.println("Load mode: Select Digit (00-99)"); }
        else if (color != -1) {
            if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
                playbackSpeed = pitchPresets[color];
                xSemaphoreGive(bufferMutex);
            }
            Serial.printf("Preset Speed: %.2f\n", playbackSpeed);
        }
        else if (irValue == IR_SAM_RECORD) {
          isRecording = !isRecording;
          if (isRecording) {
              if (adc_continuous_start(adc_handle) == ESP_OK) Serial.println("Recording...");
              else { isRecording = false; Serial.println("Failed to start ADC"); }
          }
          else { adc_continuous_stop(adc_handle); Serial.println("Stopped."); }
        }
        else if (irValue == IR_SAM_STOP) {
            if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
                activePlaybackSection = -1;
                xSemaphoreGive(bufferMutex);
            }
            Serial.println("All Stop");
        }
        else if (irValue == IR_SAM_P_UP) {
            if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
                playbackSpeed += 0.05;
                if (playbackSpeed > 4.0) playbackSpeed = 4.0;
                xSemaphoreGive(bufferMutex);
            }
            Serial.printf("Speed: %.2f\n", playbackSpeed);
        }
        else if (irValue == IR_SAM_P_DOWN) {
            if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
                playbackSpeed -= 0.05;
                if (playbackSpeed < 0.1) playbackSpeed = 0.1;
                xSemaphoreGive(bufferMutex);
            }
            Serial.printf("Speed: %.2f\n", playbackSpeed);
        }
        else {
          for (int i = 0; i < SECTIONS; i++) {
            if (irValue == PAD_CODES[i]) {
                if (xSemaphoreTake(bufferMutex, portMAX_DELAY)) {
                    activePlaybackSection = i;
                    xSemaphoreGive(bufferMutex);
                }
                Serial.printf("Playing Pad %d\n", i);
                break;
            }
          }
        }
        break;

      case WAITING_SAVE_D1:
        if (color != -1) { pitchPresets[color] = playbackSpeed; currentIRState = IDLE; Serial.println("Preset saved to color button"); }
        else if (digit != -1) { selectedFileID = digit * 10; currentIRState = WAITING_SAVE_D2; }
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
    IrReceiver.resume();
  }
}
