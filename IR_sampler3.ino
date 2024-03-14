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

// Define minimum and maximum sampling rate (adjust as needed)
const int MIN_SAMPLING_RATE = 1000;
const int MAX_SAMPLING_RATE = 10000;

// Define initial sampling rate
volatile int samplingRate = 4000;

// Define pitch shift factor (1.0 for no change)
volatile float pitchShift = 1.0;

// IR library instance
IRrecv irrecv(IR_RECEIVER_PIN);

// Flag to indicate if a sample is triggered
volatile bool sampleTriggered = false;

// Flag to indicate if a numeric key is pressed
volatile bool numericKeyPressed = false;

// Buffer to store audio samples
int sampleBuffer[SAMPLE_BUFFER_SIZE];

// Task handle for sampling task
TaskHandle_t samplingTaskHandle;

// Task handle for playback task
TaskHandle_t playbackTaskHandle;

// Function to decode IR signal
void decodeIR() {
  if (irrecv.decode(&results)) {
    if (results.decode_type == DECODE_TYP_NEC) {
      // Check for numeric keys (0-9)
      if (results.value >= 0 && results.value <= 9) {
        numericKeyPressed = true;
        sampleTriggered = true; // trigger sample for specific section 
      } else {
        // Check for other IR codes (modify for your specific needs)
        switch (results.value) {
          case SAMPLE_1_CODE:
            sampleTriggered = true;
            break;
          // ... other sample trigger codes
        }
      }
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
      numericKeyPressed = false; // reset numeric key flag
    }
    vTaskDelay(pdMS_TO_TICKS(1000 / samplingRate)); // Delay based on sampling rate
  }
}

// Function to adjust playback pitch (implement your preferred algorithm)
int adjustPitch(int sampleValue) {
  // This is a simple example of pitch shifting by a factor
  return sampleValue * pitchShift;
}

// Task to playback audio sample
void playbackTask(void *pvParameters) {
  while (true) {
    if (sampleTriggered) {
      int playbackDelay = pdMS_TO_TICKS(1000 / (samplingRate * pitchShift));
      for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        dacWrite(DAC_CHANNEL, adjustPitch(sampleBuffer[i])); // Write sample with pitch adjustment
        vTaskDelay(playbackDelay);
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
