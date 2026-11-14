#include "adc_control.h"
#include "board_pins.h"
#include <SPI.h>
#include "driver/gpio.h"

extern SPIClass spiADC;

// 2 MHz clock: 24-bit MCP3204/3208 frame completes in 12 us
static SPISettings adcSPI(2500000, MSBFIRST, SPI_MODE0);
static bool adcBusLocked = false;

uint16_t adcReadFast(uint8_t channel) {
    if (!adcBusLocked) {
        pinMode(PIN_CS_ADC, OUTPUT);
        gpio_set_level((gpio_num_t)PIN_CS_ADC, 1);
        spiADC.beginTransaction(adcSPI);
        adcBusLocked = true;
    }

    uint8_t tx[3] = {
        (uint8_t)(0x06 | ((channel >> 2) & 0x01)),
        (uint8_t)((channel & 0x03) << 6),
        0x00
    };
    uint8_t rx[3] = {0, 0, 0};

    gpio_set_level((gpio_num_t)PIN_CS_ADC, 0);
    spiADC.transferBytes(tx, rx, 3);
    gpio_set_level((gpio_num_t)PIN_CS_ADC, 1);

    return (((uint16_t)(rx[1] & 0x0F)) << 8) | rx[2];
}
