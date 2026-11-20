#ifndef DAC_CONTROL_H
#define DAC_CONTROL_H

#include <Arduino.h>

void dacBegin();
void dacWrite(uint16_t value);

#endif
