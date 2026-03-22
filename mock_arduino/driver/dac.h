#ifndef DAC_H
#define DAC_H

#include <stdint.h>

#define DAC_CHANNEL1 1
#define DAC_CHANNEL2 2
#define DAC_VREF_256 0

typedef int esp_err_t;

esp_err_t dac_output_enable(int channel);
esp_err_t dac_output_voltage(int channel, int voltage);
void dacWrite(int channel, int value);

#endif
