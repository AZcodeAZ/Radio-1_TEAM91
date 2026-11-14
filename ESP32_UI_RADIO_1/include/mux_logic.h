#ifndef MUX_LOGIC_H
#define MUX_LOGIC_H

#include <Arduino.h>

// TX Filter MUX Control Pins
constexpr uint8_t PIN_MUX_EN = 12;
constexpr uint8_t PIN_MUX_A0 = 13;
constexpr uint8_t PIN_MUX_A1 = 14;

void muxInit();
void setTxMuxEnable(bool enable);
void selectTxFilter(uint8_t channel);

#endif // MUX_LOGIC_H