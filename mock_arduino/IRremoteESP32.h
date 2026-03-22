#ifndef IRREMOTEESP32_H
#define IRREMOTEESP32_H

#include <stdint.h>

typedef uint64_t decode_type_t;
#define DECODE_TYP_NEC 1

struct decode_results {
    decode_type_t decode_type;
    uint32_t value;
};

class IRrecv {
public:
    IRrecv(int pin) {}
    void enableIRAM() {}
    bool decode(decode_results* results);
    void resume() {}
};

#endif
