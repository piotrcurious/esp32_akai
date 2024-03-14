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

// Define initial buffer size (adjust as needed)
const int SAMPLE_BUFFER_SIZE = 1024;

// Define valid IR keys (modify as needed)
const int IR_KEY_1 = 0x01;
const int IR_KEY_2 = 0x02;
const int IR_KEY_3 = 0x03;
const int IR_KEY_4 = 0x04;
const int IR_KEY_5 = 0x05;
// ... Add more keys for more sample sections

// Define playback speed range (adjust as needed)
const float MIN_PLAYBACK_SPEED = 0.5; // Half speed
const float MAX_PLAYBACK_SPEED = 2.0; // Double speed

// IR library instance
IRrecv irrecv(IR_RECEIVER_PIN);

// Flag to indicate if a sample is triggered
volatile bool sampleTriggered = false;
volatile bool isSampling = false; // Flag to indicate sampling in progress

// Buffer to store audio samples
int sampleBuffer[SAMPLE_BUFFER_SIZE];

// Current playback speed
volatile float playbackSpeed = 1.0;

// Task handle for sampling task
TaskHandle_t samplingTaskHandle;

// Task handle for playback task
TaskHandle_t playbackTaskHandle;

// Function to decode IR signal
void decodeIR() {
  if (irrecv.decode(&results)) {
    switch (results.value) {
      case IR_KEY_1:
      case IR_KEY_2:
      case IR_KEY_3:
      case IR_KEY_4:
      case IR_KEY_5:
        // Trigger sample based on key and set section index (modify)
        int sectionIndex = results.value - IR_KEY_1;
        sampleTriggered = true;
        break;
      default:
        // Handle unknown keys (optional: increase playback speed?)
        break;
    }
    irrecv.resume(); // Prepare for the next value
  }
}

// Task to sample audio input
void samplingTask(void *pvParameters) {
  while (true) {
    if (!isSampling && sampleTriggered) {
      isSampling = true;
      for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        sampleBuffer[i] = analogRead(AUDIO_INPUT_PIN); // Read analog value
      }
      isSampling = false;
      sampleTriggered = false;
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // Delay between checks
  }
}

// Task to playback audio sample with adjustable speed
void playbackTask(void *pvParameters) {
  int sampleIndex = 0;
  while (true) {
    if (sampleTriggered) {
      int delay = pdMS_TO_TICKS(10 / playbackSpeed); // Adjust delay based on speed
      for (sampleIndex = 0; sampleIndex < SAMPLE_BUFFER_SIZE; sampleIndex++) {
        dacWrite(DAC_CHANNEL, sampleBuffer[sampleIndex]); // Write sample to DAC
        vTaskDelay(delay);
      }
      sampleTriggered = false;
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // Delay between checks
  }
}

void setup() {
  Serial.begin(115200);
  irrecv.enableIRAM(); // Enable IR receiver

  // Configure DAC
  dac_output_enable(DAC_CHANNEL);
  dac_output_voltage(DAC_CHANNEL, DAC_VREF_256); // Set output voltage reference

  // Create sampling and playback tasks
  xTaskCreate(samplingTask, "SamplingTask", 2048, NULL, 1, &samplingTaskHandle);
  xTaskCreate(playbackTask, "PlaybackTask", 2048, NULL, 1, &playbackTaskHandle);
}

void loop() {
  decodeIR(); // Check for IR signal
}
