#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string>
#include <vector>

#define pdMS_TO_TICKS(ms) (ms)
#define HIGH 0x1
#define LOW  0x0
#define HEX 16

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void delay(uint32_t ms);
uint32_t millis();
int analogRead(int pin);

#ifdef __cplusplus
}
#endif

class String {
    std::string s;
public:
    String(uint32_t val, int base) {
        char buf[32];
        if (base == 16) sprintf(buf, "%x", val);
        else sprintf(buf, "%d", (int)val);
        s = buf;
    }
    const char* c_str() { return s.c_str(); }
};

class SerialMock {
public:
    void begin(int baud) {}
    void print(const char* s) { printf("%s", s); }
    void println(const char* s) { printf("%s\n", s); }
    void printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
};

extern SerialMock Serial;

void set_mock_ir_values(const std::vector<uint32_t>& values);
void run_mock_tasks(int iterations);

#endif
