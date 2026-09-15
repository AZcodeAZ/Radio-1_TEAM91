#pragma once
#include <Arduino.h>

enum class RadioMode {
    IDLE,
    TX,
    RX,
    SWITCHING,
    FAULT
};

void statusLedBegin();
void statusLedSet(RadioMode mode);
void statusLedUpdate();