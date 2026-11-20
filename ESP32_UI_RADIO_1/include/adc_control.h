#ifndef ADC_CONTROL_H
#define ADC_CONTROL_H

#include <Arduino.h>

void adcBegin();
uint16_t adcReadFast(uint8_t channel);

#endif
