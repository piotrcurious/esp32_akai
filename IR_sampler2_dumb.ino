#include <IRremoteESP32.h>
#include <driver/dac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Define IR receiver pin
const int IR_RECEIVER_PIN = 18;

// Define DAC channel for audio output
const int DAC_CHANNEL = DAC_CHANNEL1;

// Define analog input pin for audio sampling
const int AUDIO_INPUT_PIN = 34;

// Define buffer size and initial sampling rate
const int SAMPLE_BUFFER_SIZE = 1024;
const int DEFAULT_SAMPLING_RATE = 1000; // Samples per second

// Define allowed range for playback speed multiplier
const float MIN_SPEED = 0.5; // Playback is half speed
const float MAX_SPEED = 2.0; // Playback is double speed

// Define IR code for power button
const int POWER_BUTTON_CODE = 0xDDDD;
const int SPEED_UP_CODE = 0xEEEE; // Define IR code for increasing speed
const int SPEED_DOWN_CODE = 0xFFFF; // Define IR code for decreasing speed

// IR library instance
IRrecv irrecv(IR_RECEIVER_PIN);

// Flag to indicate sampling mode
volatile bool samplingEnabled = false;

// Flag to indicate if a sample is triggered
volatile bool sampleTriggered = false;

// Buffer to store audio samples
int sampleBuffer[SAMPLE_BUFFER_SIZE];

// Current sampling rate
volatile int samplingRate = DEFAULT_SAMPLING_RATE;

// Current playback speed multiplier
volatile float playbackSpeed = 1.0;

// Task handle for sampling task
TaskHandle_t samplingTaskHandle;

// Task handle for playback task
TaskHandle_t playbackTaskHandle;

// Function to decode IR signal
void decodeIR() {
  if (irrecv.decode(&results)) {
    if (results.value == POWER_BUTTON_CODE) {
      samplingEnabled = !samplingEnabled; // Toggle sampling mode
    } else if (samplingEnabled) {
      int key = results.value & 0xF; // Extract numeric key from IR code (when sampling enabled)
      if (key >= 0 && key <= 9) {
        // Calculate sample start index based on key (modify as needed)
        int startIndex = key * (SAMPLE_BUFFER_SIZE / 10);
        sampleTriggered = true;
        playbackTaskParams.startIndex = startIndex; // Pass start index to playback task
      } else if (results.value == SPEED_UP_CODE && playbackSpeed < MAX_SPEED) {
        playbackSpeed += 0.1; // Increase playback speed in steps
      } else if (results.value == SPEED_DOWN_CODE && playbackSpeed > MIN_SPEED) {
        playbackSpeed -= 0.1; // Decrease playback speed in steps
      }
    }
    irrecv.resume(); // Prepare for the next value
  }
}

// Task to sample audio input
void samplingTask(void *pvParameters) {
  while (true) {
    if (samplingEnabled) {
      if (sampleTriggered) {
        for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
          sampleBuffer[i] = analogRead(AUDIO_INPUT_PIN); // Read analog value
        }
        sampleTriggered = false;
      }
      vTaskDelay(pdMS_TO_TICKS(1000 / samplingRate)); // Delay between samples
    } else {
      vTaskDelay(pdMS_TO_TICKS(10)); // Delay while not in sampling mode
    }
  }
}

// Task to playback audio sample with adjustable speed
void playbackTask(void *pvParameters) {
  PlaybackTaskParams* params = (PlaybackTaskParams*) pvParameters;
  int startIndex = params->startIndex;
  int endIndex = startIndex + (SAMPLE_BUFFER_SIZE / 10); // Assuming 10 sections

  int playbackDelay = pdMS_TO_TICKS(1000 / (samplingRate * playbackSpeed)); // Calculate delay based on speed

  while (true) {
    if (samplingEnabled && sampleTriggered) {
      for (int i = startIndex; i < endIndex; i++) {
        int sample = sampleBuffer[i];
        dacWrite
