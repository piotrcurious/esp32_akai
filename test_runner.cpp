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

void samplingTaskTest() {
  static int writeIndex = 0;
  if (isRecording) {
    sampleBuffer[writeIndex] = (uint8_t)(analogRead(AUDIO_INPUT_PIN) >> 4);
    writeIndex = (writeIndex + 1) % SAMPLE_BUFFER_SIZE;
  } else {
    writeIndex = 0;
  }
}

void test_flash_workflow() {
    std::cout << "Testing Flash Save/Load Workflow (Samsung Remote)..." << std::endl;
    isRecording = false;
    for(int i=0; i<SAMPLE_BUFFER_SIZE; i++) sampleBuffer[i] = 0;

    // 1. Record something
    isRecording = true;
    samplingTaskTest();
    assert(sampleBuffer[0] == 128);
    isRecording = false;

    // 2. Save as File 42 (Source -> 4 -> 2)
    set_mock_ir_values({IR_SAM_SOURCE, DIGIT_CODES[4], DIGIT_CODES[2]});
    loop(); loop(); loop();
    std::cout << "Save Sequence Completed!" << std::endl;

    // 3. Clear buffer
    for(int i=0; i<SAMPLE_BUFFER_SIZE; i++) sampleBuffer[i] = 0;
    assert(sampleBuffer[0] == 0);

    // 4. Load File 42 (Subtitle -> 4 -> 2)
    set_mock_ir_values({IR_SAM_SUBTITLE, DIGIT_CODES[4], DIGIT_CODES[2]});
    loop(); loop(); loop();
    assert(sampleBuffer[0] == 128);
    std::cout << "Load Sequence Completed and Verified!" << std::endl;
}

int main() {
    setup();
    test_flash_workflow();
    std::cout << "All Storage MPC Tests Passed!" << std::endl;
    return 0;
}
