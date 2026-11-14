#include "dac_control.h"
#include "board_pins.h"
#include <SPI.h>
#include "driver/gpio.h"

extern SPIClass spiDAC;

// 20 MHz, MSB first, SPI_MODE1 (DAC121S101 latches on falling SCLK edge)
static SPISettings dacSPI(20000000, MSBFIRST, SPI_MODE1);

void dacBegin() {
    pinMode(PIN_CS_DAC, OUTPUT);
    gpio_set_level((gpio_num_t)PIN_CS_DAC, 1);

    // Acquire HSPI bus once (no other device shares spiDAC)
    spiDAC.beginTransaction(dacSPI);

    // Initialize output to mid-scale silence (2048 -> 1.65V)
    dacWrite(2048);
}

void dacWrite(uint16_t value) {
    // 16-bit frame: [0, 0, PD1=0, PD0=0, D11..D0]
    uint16_t frame = value & 0x0FFF;

    gpio_set_level((gpio_num_t)PIN_CS_DAC, 0);
    spiDAC.transfer16(frame);
    gpio_set_level((gpio_num_t)PIN_CS_DAC, 1);
}
