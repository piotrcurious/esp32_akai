#include "mock_arduino/Arduino.h"
#include "mock_arduino/IRremoteESP32.h"
#include <assert.h>
#include <vector>
#include <iostream>

#define TESTING_MOCK 1
#include "IR_sampler_final.ino"

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

void test_recording_and_content() {
    std::cout << "Testing Recording and Content..." << std::endl;
    isRecording = false;
    for(int i=0; i<SAMPLE_BUFFER_SIZE; i++) sampleBuffer[i] = 0;

    set_mock_ir_values({IR_RECORD});
    loop();
    assert(isRecording == true);

    // Run test sampling task
    samplingTaskTest();

    assert(sampleBuffer[0] == 128); // 2048 >> 4

    set_mock_ir_values({IR_RECORD});
    loop();
    assert(isRecording == false);
    std::cout << "Recording Content Verified!" << std::endl;
}

void test_playback_trigger() {
    std::cout << "Testing Playback Trigger..." << std::endl;
    activePlaybackSection = -1;
    set_mock_ir_values({0xFF30CF}); // IR_PLAY_1
    loop();
    assert(activePlaybackSection == 0);
    std::cout << "Playback Triggered for Section 0!" << std::endl;
}

int main() {
    setup();
    test_recording_and_content();
    test_playback_trigger();
    std::cout << "All Improved Tests Passed!" << std::endl;
    return 0;
}
