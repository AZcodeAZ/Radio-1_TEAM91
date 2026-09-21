#include "dac_control.h"
#include "board_pins.h"
#include <SPI.h>

static SPISettings dacSPI(
    20000000,
    MSBFIRST,
    SPI_MODE2
);

void dacBegin()
{
    pinMode(PIN_CS_DAC, OUTPUT);
    digitalWrite(PIN_CS_DAC, HIGH);

    pinMode(PIN_CS_ADC, OUTPUT);
    digitalWrite(PIN_CS_ADC, HIGH);

    SPI.begin(
        PIN_SPI_SCK,
        PIN_SPI_MISO,
        PIN_SPI_MOSI,
        -1
    );

    // AD5628 Reference Command: 
    // Command (DB27-DB24) = 1000 (0x8)
    // DB0 = 1 (Turn internal reference ON)
    uint32_t refCommand = (0x8UL << 24) | 0x00000001UL;

    SPI.beginTransaction(dacSPI);
    digitalWrite(PIN_CS_DAC, LOW);

    SPI.transfer((refCommand >> 24) & 0xFF);
    SPI.transfer((refCommand >> 16) & 0xFF);
    SPI.transfer((refCommand >> 8) & 0xFF);
    SPI.transfer(refCommand & 0xFF);

    digitalWrite(PIN_CS_DAC, HIGH);
    SPI.endTransaction();

    delay(10);
}

void dacWrite(uint16_t value)
{
    value &= 0x0FFF;

    uint32_t command =
        (0x3UL << 24) |
        ((uint32_t)value << 8);

    uint8_t txData[4];

    txData[0] = (command >> 24) & 0xFF;
    txData[1] = (command >> 16) & 0xFF;
    txData[2] = (command >> 8) & 0xFF;
    txData[3] = command & 0xFF;

    SPI.beginTransaction(dacSPI);

    digitalWrite(PIN_CS_DAC, LOW);

    SPI.transferBytes(txData, nullptr, 4);

    digitalWrite(PIN_CS_DAC, HIGH);

    SPI.endTransaction();
}

void dacBeginContinuous()
{
    SPI.beginTransaction(dacSPI);
}

void dacWriteContinuous(uint16_t value)
{
    value &= 0x0FFF;

    uint32_t command =
        (0x3UL << 24) |
        ((uint32_t)value << 8);

    uint8_t txData[4];

    txData[0] = (command >> 24) & 0xFF;
    txData[1] = (command >> 16) & 0xFF;
    txData[2] = (command >> 8) & 0xFF;
    txData[3] = command & 0xFF;

    digitalWrite(PIN_CS_DAC, LOW);

    SPI.transferBytes(txData, nullptr, 4);

    digitalWrite(PIN_CS_DAC, HIGH);
}

void dacEndContinuous()
{
    SPI.endTransaction();
}