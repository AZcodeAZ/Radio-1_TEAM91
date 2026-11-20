#pragma once
#include <Arduino.h>

#if CONFIG_IDF_TARGET_ESP32H2
// ==========================================
// ESP32-H2-DevKitM-1 v1.2 Pinout (RX Loop-Test)
// ==========================================
// Hardware FSPI (J1 Header)
constexpr uint8_t PIN_ADC_SCK       = 4;
constexpr uint8_t PIN_ADC_MISO      = 0;
constexpr uint8_t PIN_ADC_MOSI      = 5;
constexpr uint8_t PIN_CS_ADC        = 1;

// Safe J3 Header GPIOs (avoids strapping/USB pins 8, 9, 25, 26, 27)
constexpr uint8_t PIN_TX_EN         = 10;
constexpr uint8_t PIN_TX_A0         = 11;
constexpr uint8_t PIN_TX_A1         = 12;
constexpr uint8_t PIN_CS_DAC        = 22;
constexpr uint8_t PIN_DAC_MOSI      = 5;
constexpr uint8_t PIN_DAC_SCK       = 4;
constexpr uint8_t PIN_BATTERY_SENSE = 13;
constexpr uint8_t PIN_RF_SWITCH     = 14;

#else
// ==========================================
// ESP32-S3-WROOM-2 Pinout (Primary Radio)
// ==========================================
constexpr uint8_t PIN_TX_EN         = 12;
constexpr uint8_t PIN_TX_A0         = 13;
constexpr uint8_t PIN_TX_A1         = 14;

constexpr uint8_t PIN_CS_DAC        = 11;
constexpr uint8_t PIN_DAC_MOSI      = 10;
constexpr uint8_t PIN_DAC_SCK       = 9;

constexpr uint8_t PIN_ADC_SCK       = 21;
constexpr uint8_t PIN_ADC_MOSI      = 2;
constexpr uint8_t PIN_ADC_MISO      = 47;
constexpr uint8_t PIN_CS_ADC        = 1;

constexpr uint8_t PIN_BATTERY_SENSE = 5;
constexpr uint8_t PIN_RF_SWITCH     = 8;
#endif
