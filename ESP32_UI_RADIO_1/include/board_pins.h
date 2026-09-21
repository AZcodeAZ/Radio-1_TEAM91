#pragma once
#include <Arduino.h>

//CD4051BE Mulitplexers
constexpr uint8_t PIN_MUX_A = 4;
constexpr uint8_t PIN_MUX_B = 5;
constexpr uint8_t PIN_MUX_C = 6;
constexpr uint8_t PIN_MUX_INH = 7;

//SPI Bus (Shared: Pmod DA4, MCP3208)
constexpr uint8_t PIN_SPI_SCK = 12;
constexpr uint8_t PIN_SPI_MOSI = 11;
constexpr uint8_t PIN_SPI_MISO = 13;
constexpr uint8_t PIN_CS_DAC = 9;
constexpr uint8_t PIN_CS_ADC = 10;

//I2C Bus (PCF8574P)
constexpr uint8_t PIN_I2C_SDA = 1;
constexpr uint8_t PIN_I2C_SLC = 2;