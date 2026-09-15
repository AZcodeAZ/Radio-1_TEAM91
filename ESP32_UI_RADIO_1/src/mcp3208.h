#pragma once
#include <Arduino.h>

class MCP3208 {
public:
    void begin();
    uint16_t readChannel(uint8_t channel);
};