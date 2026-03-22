#include "mock_arduino/Arduino.h"
#include "mock_arduino/IRremoteESP32.h"
#include "mock_arduino/LittleFS.h"
#include <assert.h>
#include <vector>
#include <iostream>

#define TESTING_MOCK 1
#include "IR_sampler_final.ino"

void samplingTask(void *pvParameters) {}
void playbackTask(void *pvParameters) {}

void test_presets_workflow() {
    std::cout << "Testing Pitch Presets Workflow..." << std::endl;
    playbackSpeed = 2.5;

    // 1. Save Preset A (Source -> Red)
    set_mock_ir_values({IR_SAM_SOURCE, COLOR_CODES[0]});
    loop(); loop();
    assert(pitchPresets[0] == 2.5);
    std::cout << "Preset A Saved!" << std::endl;

    // 2. Change Speed and Load Preset A (Red)
    playbackSpeed = 1.0;
    set_mock_ir_values({COLOR_CODES[0]});
    loop();
    assert(playbackSpeed == 2.5);
    std::cout << "Preset A Loaded!" << std::endl;

    // 3. Save as File 11 (Source -> 1 -> 1)
    set_mock_ir_values({IR_SAM_SOURCE, DIGIT_CODES[1], DIGIT_CODES[1]});
    loop(); loop(); loop();
    std::cout << "File 11 Saved with Presets!" << std::endl;

    // 4. Clear presets and Speed
    pitchPresets[0] = 0.0;
    playbackSpeed = 0.0;

    // 5. Load File 11 (Subtitle -> 1 -> 1)
    set_mock_ir_values({IR_SAM_SUBTITLE, DIGIT_CODES[1], DIGIT_CODES[1]});
    loop(); loop(); loop();
    assert(pitchPresets[0] == 2.5);
    std::cout << "File 11 Loaded and Presets Verified!" << std::endl;
}

int main() {
    setup();
    test_presets_workflow();
    std::cout << "All Pitch Preset MPC Tests Passed!" << std::endl;
    return 0;
}
