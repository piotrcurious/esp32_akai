#include "mock_arduino/Arduino.h"
#include "mock_arduino/IRremoteESP32.h"
#include <assert.h>
#include <vector>
#include <iostream>

#define TESTING_MOCK 1
#include "IR_sampler_final.ino"

// Define these for mocking setup
void samplingTask(void *pvParameters) {}
void playbackTask(void *pvParameters) {}

// Test version of samplingTask
void samplingTaskTest() {
  static int writeIndex = 0;
  if (isRecording) {
    sampleBuffer[writeIndex] = (uint8_t)(analogRead(AUDIO_INPUT_PIN) >> 4);
    writeIndex = (writeIndex + 1) % SAMPLE_BUFFER_SIZE;
  } else {
    writeIndex = 0;
  }
}

void test_mpc_workflow() {
    std::cout << "Testing MPC Workflow (Samsung Remote)..." << std::endl;
    isRecording = false;
    activePlaybackSection = -1;
    for(int i=0; i<SAMPLE_BUFFER_SIZE; i++) sampleBuffer[i] = 0;

    // 1. Toggle Record
    set_mock_ir_values({IR_SAM_RECORD});
    loop();
    assert(isRecording == true);
    std::cout << "Record Enabled!" << std::endl;

    // 2. Fill Pad 1 (Section 0)
    samplingTaskTest();
    assert(sampleBuffer[0] == 128); // 2048 >> 4

    // 3. Trigger Pad 1
    set_mock_ir_values({PAD_CODES[0]}); // Pad 1 (1)
    loop();
    assert(activePlaybackSection == 0);
    std::cout << "Pad 1 Triggered!" << std::endl;

    // 4. Pitch Down
    float initialSpeed = playbackSpeed;
    set_mock_ir_values({IR_SAM_P_DOWN});
    loop();
    assert(playbackSpeed < initialSpeed);
    std::cout << "Pitch Down: " << playbackSpeed << std::endl;

    // 5. Stop All
    set_mock_ir_values({IR_SAM_STOP});
    loop();
    assert(activePlaybackSection == -1);
    std::cout << "All Playback Stopped!" << std::endl;

    std::cout << "MPC Workflow Verified!" << std::endl;
}

int main() {
    setup();
    test_mpc_workflow();
    std::cout << "All Improved MPC Tests Passed!" << std::endl;
    return 0;
}
