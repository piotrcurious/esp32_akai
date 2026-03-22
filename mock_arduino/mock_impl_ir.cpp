#include "mock_arduino/IRremoteESP32.h"
#include <vector>

IrReceiverMock IrReceiver;

void set_mock_ir_values(const std::vector<uint32_t>& values) {
    IrReceiver.set_mock_values(values);
}
