
#pragma once

#include <Arduino.h>

void dacBegin();
void dacWrite(uint16_t value);
void dacBeginContinuous();
void dacWriteContinuous(uint16_t value);
void dacEndContinuous();