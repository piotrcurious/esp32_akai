#ifndef IRREMOTEESP32_H
#define IRREMOTEESP32_H

#include <stdint.h>
#include "Arduino.h"

#define ENABLE_LED_FEEDBACK true
#define DISABLE_LED_FEEDBACK false

typedef struct {
    uint32_t decodedRawData;
} IRData;

class IrReceiverMock {
public:
    IRData decodedIRData;
    void begin(int pin, bool feedback) {}
    bool decode() {
        if (mock_values.empty()) return false;
        decodedIRData.decodedRawData = mock_values.front();
        mock_values.erase(mock_values.begin());
        return true;
    }
    void resume() {}

    void set_mock_values(const std::vector<uint32_t>& values) {
        mock_values = values;
    }
private:
    std::vector<uint32_t> mock_values;
};

extern IrReceiverMock IrReceiver;

#endif
