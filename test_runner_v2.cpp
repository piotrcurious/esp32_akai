#include "mock_arduino/Arduino.h"
#include "mock_arduino/IRremoteESP32.h"
#include "mock_arduino/LittleFS.h"
#include <assert.h>
#include <vector>
#include <iostream>
#include <cmath>

// Mock DMA functions
typedef enum {
    ESP_OK = 0,
    ESP_FAIL = -1
} esp_err_t;

typedef void* adc_continuous_handle_t;
typedef void* dac_continuous_handle_t;
typedef uint32_t adc_channel_t;
#define ADC_CHANNEL_6 6
typedef uint32_t dac_channel_mask_t;
#define DAC_CHANNEL_MASK_CH0 1
#define ADC_UNIT_1 1
#define ADC_BITWIDTH_12 12
#define ADC_CONV_SINGLE_UNIT_1 1
#define ADC_DIGI_OUTPUT_FORMAT_TYPE1 1
#define DAC_DIGI_CLK_SRC_DEFAULT 0
#define DAC_CHANNEL_MODE_SIMUL 0
#define ADC_ATTEN_DB_11 3

typedef struct {
    uint32_t atten;
    uint32_t channel;
    uint32_t unit;
    uint32_t bit_width;
} adc_digi_pattern_config_t;

typedef struct {
    uint32_t max_store_buf_size;
    uint32_t conv_frame_size;
} adc_continuous_handle_cfg_t;

typedef struct {
    uint32_t pattern_num;
    adc_digi_pattern_config_t *adc_pattern;
    uint32_t sample_freq_hz;
    uint32_t conv_mode;
    uint32_t format;
} adc_continuous_config_t;

typedef struct {
    uint32_t chan_mask;
    uint32_t desc_num;
    uint32_t desc_size;
    uint32_t sample_freq_hz;
    int offset;
    uint32_t clk_src;
    uint32_t chan_mode;
} dac_continuous_config_t;

typedef struct {
    union {
        struct {
            uint16_t data : 12;
            uint16_t channel : 4;
        } type1;
        uint16_t val;
    };
} adc_digi_output_data_t;

esp_err_t adc_continuous_new_handle(adc_continuous_handle_cfg_t *hcfg, adc_continuous_handle_t *ret_handle) { return ESP_OK; }
esp_err_t adc_continuous_config(adc_continuous_handle_t handle, adc_continuous_config_t *config) { return ESP_OK; }
esp_err_t adc_continuous_start(adc_continuous_handle_t handle) { return ESP_OK; }
esp_err_t adc_continuous_stop(adc_continuous_handle_t handle) { return ESP_OK; }
esp_err_t adc_continuous_read(adc_continuous_handle_t handle, uint8_t *buf, uint32_t length_max, uint32_t *out_length, uint32_t timeout_ms) {
    *out_length = 0;
    return ESP_OK;
}

esp_err_t dac_continuous_new_channels(dac_continuous_config_t *cfg, dac_continuous_handle_t *ret_handle) { return ESP_OK; }
esp_err_t dac_continuous_enable(dac_continuous_handle_t handle) { return ESP_OK; }
esp_err_t dac_continuous_start(dac_continuous_handle_t handle) { return ESP_OK; }
esp_err_t dac_continuous_write(dac_continuous_handle_t handle, uint8_t *buf, uint32_t length, size_t *out_length, uint32_t timeout_ms) {
    *out_length = length;
    return ESP_OK;
}

#define TESTING_MOCK 1

#include "IR_sampler_v2_dma.ino"

bool is_near(float a, float b) {
    return std::abs(a - b) < 0.0001;
}

void test_full_review() {
    std::cout << "Starting Full Review Test for DMA version (v2.1)..." << std::endl;
    setup();

    // Test 1: Pads
    activePlaybackSection = -1;
    set_mock_ir_values({PAD_CODES[0], PAD_CODES[7]});
    loop(); assert(activePlaybackSection == 0);
    loop(); assert(activePlaybackSection == 7);
    std::cout << "Pads 1 & 8 Verified" << std::endl;

    // Test 2: Pitch Control
    playbackSpeed = 1.0;
    set_mock_ir_values({IR_SAM_P_UP, IR_SAM_P_DOWN});
    loop(); assert(playbackSpeed > 1.0);
    loop(); assert(is_near(playbackSpeed, 1.0));
    std::cout << "Pitch Control Verified" << std::endl;

    // Test 3: Pitch Presets (Color Keys)
    playbackSpeed = 2.0;
    set_mock_ir_values({IR_SAM_SOURCE, COLOR_CODES[1]}); // Source + Green
    loop(); loop();
    assert(is_near(pitchPresets[1], 2.0));

    playbackSpeed = 1.0;
    set_mock_ir_values({COLOR_CODES[1]}); // Green
    loop();
    assert(is_near(playbackSpeed, 2.0));
    std::cout << "Pitch Presets Verified" << std::endl;

    // Test 4: Flash Save/Load
    for(int i=0; i<SAMPLE_BUFFER_SIZE; i++) sampleBuffer[i] = 0;
    sampleBuffer[0] = 128;

    set_mock_ir_values({IR_SAM_SOURCE, DIGIT_CODES[9], DIGIT_CODES[9]}); // Save 99
    loop(); loop(); loop();

    // Reset state
    for(int i=0; i<SAMPLE_BUFFER_SIZE; i++) sampleBuffer[i] = 0;
    pitchPresets[1] = 0.0;

    set_mock_ir_values({IR_SAM_SUBTITLE, DIGIT_CODES[9], DIGIT_CODES[9]}); // Load 99
    loop(); loop(); loop();

    assert(sampleBuffer[0] == 128);
    assert(is_near(pitchPresets[1], 2.0));
    std::cout << "Persistence (Sample + Presets) Verified" << std::endl;

    std::cout << "Full Review Completed Successfully!" << std::endl;
}

int main() {
    test_full_review();
    return 0;
}
