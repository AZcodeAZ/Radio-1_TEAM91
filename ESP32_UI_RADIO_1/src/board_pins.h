#pragma once
#include <Arduino.h>

// Shared SPI bus: Pmod DA4 + MCP3208
static constexpr gpio_num_t PIN_SPI_SCK  = GPIO_NUM_12;
static constexpr gpio_num_t PIN_SPI_MOSI = GPIO_NUM_11;
static constexpr gpio_num_t PIN_SPI_MISO = GPIO_NUM_13;

static constexpr gpio_num_t PIN_DA4_CS      = GPIO_NUM_10;
static constexpr gpio_num_t PIN_MCP3208_CS  = GPIO_NUM_9;

// RF controls. Do not connect these to the RF PCB until you verify
// your schematic's mapping of J1/J2/J3
static constexpr gpio_num_t PIN_SWITCH_V1   = GPIO_NUM_7;
static constexpr gpio_num_t PIN_SWITCH_V2   = GPIO_NUM_6;

static constexpr gpio_num_t PIN_RGB_LED = GPIO_NUM_38;