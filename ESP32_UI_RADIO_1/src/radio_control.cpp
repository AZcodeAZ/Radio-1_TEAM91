#include "radio_control.h"
#include "board_pins.h"
#include "da4.h"

extern DA4 da4;

static RadioMode currentMode = RadioMode::IDLE;

static void switchToTxPath() {
    // AS183 truth table:
    // V1 = HIGH, V2 = LOW connects J1 to J2.
    // This assumes your PCB routes J1-J2 as antenna-to-TX.
    digitalWrite(PIN_SWITCH_V1, HIGH);
    digitalWrite(PIN_SWITCH_V2, LOW);
}

static void switchToRxPath() {
    // V1 = LOW, V2 = HIGH connects J1 to J3.
    // This assumes your PCB routes J1-J3 as antenna-to-RX.
    digitalWrite(PIN_SWITCH_V1, LOW);
    digitalWrite(PIN_SWITCH_V2, HIGH);
}

void radioControlBegin() {
    pinMode(PIN_PA_EN, OUTPUT);
    pinMode(PIN_SWITCH_V1, OUTPUT);
    pinMode(PIN_SWITCH_V2, OUTPUT);

    // Safe startup: PA off and one valid switch state selected.
    digitalWrite(PIN_PA_EN, LOW);
    switchToRxPath();

    currentMode = RadioMode::IDLE;
    statusLedSet(currentMode);
}

RadioMode radioGetMode() {
    return currentMode;
}

void radioSetMode(RadioMode requestedMode) {
    if (requestedMode == currentMode) {
        return;
    }

    // Never drive PA during a switching event.
    digitalWrite(PIN_PA_EN, LOW);
    da4.setMidpoint();
    currentMode = RadioMode::SWITCHING;
    statusLedSet(currentMode);
    delay(2);

    switch (requestedMode) {
    case RadioMode::TX:
        switchToTxPath();
        delay(2);
        currentMode = RadioMode::TX;
        break;

    case RadioMode::RX:
        switchToRxPath();
        delay(2);
        currentMode = RadioMode::RX;
        break;

    case RadioMode::IDLE:
    default:
        switchToRxPath();
        currentMode = RadioMode::IDLE;
        break;
}

    statusLedSet(currentMode);
}