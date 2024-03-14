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

// Define initial buffer size and sampling rate
const int SAMPLE_BUFFER_SIZE = 1024;
const int DEFAULT_SAMPLING_RATE = 1000; // Samples per second

// Define allowed range for playback pitch
const float MIN_PITCH = 0.5;
const float MAX_PITCH = 2.0;

// IR library instance
IRrecv irrecv(IR_RECEIVER_PIN);

// Flag to indicate if a sample is triggered
volatile bool sampleTriggered = false;

// Buffer to store audio samples
int sampleBuffer[SAMPLE_BUFFER_SIZE];

// Current sampling rate
volatile int samplingRate = DEFAULT_SAMPLING_RATE;

// Current playback pitch
volatile float playbackPitch = 1.0;

// Task handle for sampling task
TaskHandle_t samplingTaskHandle;

// Task handle for playback task
TaskHandle_t playbackTaskHandle;

// Function to decode IR signal
void decodeIR() {
  if (irrecv.decode(&results)) {
    int key = results.value & 0xF; // Extract numeric key from IR code
    if (key >= 0 && key <= 9) {
      // Calculate sample start index based on key (modify as needed)
      int startIndex = key * (SAMPLE_BUFFER_SIZE / 10);
      sampleTriggered = true;
      playbackTaskParams.startIndex = startIndex; // Pass start index to playback task
    }
    irrecv.resume(); // Prepare for the next value
  }
}

// Task to sample audio input
void samplingTask(void *pvParameters) {
  while (true) {
    if (sampleTriggered) {
      for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        sampleBuffer[i] = analogRead(AUDIO_INPUT_PIN); // Read analog value
      }
      sampleTriggered = false;
    }
    vTaskDelay(pdMS_TO_TICKS(1000 / samplingRate)); // Delay between samples
  }
}

// Task to playback audio sample with adjustable pitch
void playbackTask(void *pvParameters) {
  PlaybackTaskParams* params = (PlaybackTaskParams*) pvParameters;
  int startIndex = params->startIndex;
  int endIndex = startIndex + (SAMPLE_BUFFER_SIZE / 10); // Assuming 10 sections

  while (true) {
    if (sampleTriggered) {
      for (int i = startIndex; i < endIndex; i++) {
        int sample = sampleBuffer[i];
        // Apply pitch shift (adjust formula as needed)
        int shiftedSample = sample * playbackPitch;
        dacWrite(DAC_CHANNEL, shiftedSample);
      }
      vTaskDelay(pdMS_TO_TICKS(1000 / (samplingRate * playbackPitch))); // Delay for playback considering pitch
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // Delay between checks
  }
}

struct PlaybackTaskParams {
  int startIndex;
};

void setup() {
  Serial.begin(115200);
  irrecv.enableIRAM(); // Enable IR receiver

  // Configure DAC
  dac_output_enable(DAC_CHANNEL);
  dac_output_voltage(DAC_CHANNEL, DAC_VREF_256); // Set output voltage reference

  // Create sampling and playback tasks with parameters
  PlaybackTaskParams playbackTaskParams;
  playbackTaskParams.startIndex = 0; // Default start index
  xTaskCreate(samplingTask, "SamplingTask", 2048, NULL, 1, &samplingTaskHandle);
  xTaskCreate(playbackTask, "PlaybackTask", 2048, &playbackTaskParams, 1, &playbackTaskHandle);
}

void loop() {
  decodeIR(); // Check for IR signal
}
