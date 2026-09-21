#include "adc_control.h"
#include "board_pins.h"
#include <SPI.h>

// MCP3208 SPI Configuration
// Max clock speed: 2 MHz at 5V / 1 MHz at 2.7V
static SPISettings adcSPI(
    2000000,
    MSBFIRST,
    SPI_MODE0
);

void adcBegin()
{
    pinMode(PIN_CS_ADC, OUTPUT);
    digitalWrite(PIN_CS_ADC, HIGH);

    pinMode(PIN_CS_DAC, OUTPUT);
    digitalWrite(PIN_CS_DAC, HIGH);

    SPI.begin(
        PIN_SPI_SCK,
        PIN_SPI_MISO,
        PIN_SPI_MOSI,
        -1
    );

    delay(10);
}

uint16_t adcRead(uint8_t channel)
{
    if (channel > 7)
    {
        channel = 7;
    }

    // MCP3208 24-bit SPI Frame:
    // Byte 0: [0 0 0 0 0 1 SGL/DIFF D2] -> 0x06 | ((channel >> 2) & 0x01)
    // Byte 1: [D1 D0 X X X X X X]       -> (channel & 0x03) << 6
    // Byte 2: [X X X X X X X X]         -> 0x00
    uint8_t txData[3];
    uint8_t rxData[3] = {0};

    txData[0] = 0x06 | ((channel >> 2) & 0x01);
    txData[1] = (channel & 0x03) << 6;
    txData[2] = 0x00;

    SPI.beginTransaction(adcSPI);

    digitalWrite(PIN_CS_ADC, LOW);

    SPI.transferBytes(txData, rxData, 3);

    digitalWrite(PIN_CS_ADC, HIGH);

    SPI.endTransaction();

    // Extract 12-bit result from received bytes
    uint16_t adcValue =
        ((uint16_t)(rxData[1] & 0x0F) << 8) | rxData[2];

    return adcValue;
}

void adcBeginContinuous()
{
    SPI.beginTransaction(adcSPI);
}

uint16_t adcReadContinuous(uint8_t channel)
{
    if (channel > 7)
    {
        channel = 7;
    }

    uint8_t txData[3];
    uint8_t rxData[3] = {0};

    txData[0] = 0x06 | ((channel >> 2) & 0x01);
    txData[1] = (channel & 0x03) << 6;
    txData[2] = 0x00;

    digitalWrite(PIN_CS_ADC, LOW);

    SPI.transferBytes(txData, rxData, 3);

    digitalWrite(PIN_CS_ADC, HIGH);

    return ((uint16_t)(rxData[1] & 0x0F) << 8) | rxData[2];
}

void adcEndContinuous()
{
    SPI.endTransaction();
}