#include "status_led.h"
#include "board_pins.h"
#include <Adafruit_NeoPixel.h>

static constexpr uint8_t RGB_LED_COUNT = 1;
static constexpr uint8_t LED_BRIGHTNESS = 20;

Adafruit_NeoPixel statusLed(
    RGB_LED_COUNT,
    PIN_RGB_LED,
    NEO_GRB + NEO_KHZ800
);

static RadioMode currentMode = RadioMode::IDLE;
static uint32_t lastBlinkMs = 0;
static bool blinkOn = false;

static void showColor(uint8_t red, uint8_t green, uint8_t blue) {
    statusLed.setPixelColor(0, statusLed.Color(red, green, blue));
    statusLed.show();
}

void statusLedBegin() {
    statusLed.begin();
    statusLed.setBrightness(LED_BRIGHTNESS);
    statusLed.clear();
    statusLed.show();

    statusLedSet(RadioMode::IDLE);
}

void statusLedSet(RadioMode mode) {
    currentMode = mode;

    switch (currentMode) {
        case RadioMode::IDLE:
            showColor(0, 0, 255);       // Blue
            break;

        case RadioMode::TX:
            showColor(0, 255, 0);       // Green
            break;

        case RadioMode::RX:
            showColor(255, 110, 0);     // Amber: red + reduced green
            break;

        case RadioMode::SWITCHING:
            blinkOn = false;
            lastBlinkMs = 0;
            showColor(0, 0, 0);
            break;

        case RadioMode::FAULT:
            showColor(255, 0, 0);       // Red
            break;
    }
}

void statusLedUpdate() {
    if (currentMode != RadioMode::SWITCHING) {
        return;
    }

    const uint32_t now = millis();

    if (now - lastBlinkMs < 150) {
        return;
    }

    lastBlinkMs = now;
    blinkOn = !blinkOn;

    if (blinkOn) {
        showColor(255, 110, 0);         // Flash amber while switching
    } else {
        showColor(0, 0, 0);
    }
}