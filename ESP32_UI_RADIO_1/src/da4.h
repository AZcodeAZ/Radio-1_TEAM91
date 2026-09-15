#pragma once
#include <Arduino.h>

class DA4 {
public:
    void begin();
    void enableInternalReference();
    void writeChannelA(uint16_t code);
    void setMidpoint();
    void powerDownAll();

private:
    void writeCommand(uint32_t command);
};