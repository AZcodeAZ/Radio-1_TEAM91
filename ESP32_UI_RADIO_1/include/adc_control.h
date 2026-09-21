#pragma once

#include <Arduino.h>

void adcBegin();
uint16_t adcRead(uint8_t channel);
void adcBeginContinuous();
uint16_t adcReadContinuous(uint8_t channel);
void adcEndContinuous();