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

// Define allowed range for playback pitch
const float MIN_PITCH = 0.5;
const float MAX_PITCH = 2.0;

// Define IR code for power button
const int POWER_BUTTON_CODE = 0xDDDD;

// IR library instance
IRrecv irrecv(IR_RECEIVER_PIN);

// Flag to indicate sampling mode
volatile bool samplingEnabled = false;

// Flag to indicate if a sample is triggered
volatile bool sampleTriggered = false;

// Buffer to store audio samples
int sampleBuffer[SAMPLE_BUFFER_SIZE];

// Current desired playback pitch
volatile float desiredPlaybackPitch = 1.0;

// Variables for Bresenham-inspired timing
volatile int playbackSampleIndex = 0;
volatile int playbackErrorTerm = 0;
volatile int playbackStep = int(DEFAULT_SAMPLING_RATE * desiredPlaybackPitch); // Initial step size

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
      vTaskDelay(pdMS_TO_TICKS(1000 / DEFAULT_SAMPLING_RATE)); // Delay between samples (original rate)
    } else {
      vTaskDelay(pdMS_TO_TICKS(10)); // Delay while not in sampling mode
    }
  }
}

// Task to playback audio sample with Bresenham-inspired timing
void playbackTask(void *pvParameters) {
  PlaybackTaskParams* params = (PlaybackTaskParams*) pvParameters;
  int startIndex = params->startIndex;
  int endIndex = startIndex + (SAMPLE_BUFFER_SIZE / 10); // Assuming 10 sections

  while (true) {
    if (samplingEnabled && sampleTriggered) {
      if (playbackSampleIndex < endIndex) {
        int sample = sampleBuffer[playbackSampleIndex];
        // Apply pitch shift (adjust formula as needed)
        int shiftedSample = sample * desiredPlaybackPitch;
        dacWrite(DAC_CHANNEL, shiftedSample);

        // Bresenham-inspired timing adjustment
        playbackErrorTerm += playbackStep;
        if (playbackErrorTerm >= (DEFAULT_SAMPLING_RATE * desiredPlaybackPitch) / 2) {
          playbackSampleIndex++;
          playbackErrorTerm -= (DEFAULT_SAMPLING_RATE * desiredPlaybackPitch);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1)); // Minimal delay for calculations
    } else {
      vTaskDelay(pdMS_TO_TICKS(10
        
