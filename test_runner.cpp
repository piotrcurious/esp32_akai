#include "mock_arduino/Arduino.h"
#include "mock_arduino/IRremoteESP32.h"
#include "mock_arduino/LittleFS.h"
#include <assert.h>
#include <vector>
#include <iostream>
#include <cmath>

#define TESTING_MOCK 1
#include "IR_sampler_final.ino"

void samplingTask(void *pvParameters) {}
void playbackTask(void *pvParameters) {}

static int testWriteIndex = 0;
void samplingTaskTest() {
  if (isRecording) {
    sampleBuffer[testWriteIndex] = 128; // Manually set for test
    testWriteIndex = (testWriteIndex + 1) % SAMPLE_BUFFER_SIZE;
  } else {
    testWriteIndex = 0;
  }
}

bool is_near(float a, float b) {
    return std::abs(a - b) < 0.0001;
}

void test_full_review() {
    std::cout << "Starting Full Review Test..." << std::endl;
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
    isRecording = true;
    testWriteIndex = 0;
    samplingTaskTest();
    isRecording = false;
    assert(sampleBuffer[0] == 128);

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
