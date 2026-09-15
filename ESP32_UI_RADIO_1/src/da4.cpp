#include "da4.h"
#include "board_pins.h"
#include <SPI.h>

extern SPIClass radioSPI;

static constexpr uint32_t DA4_SPI_CLOCK_HZ = 8000000;

static constexpr uint8_t CMD_WRITE_AND_UPDATE = 0x03;
static constexpr uint8_t CMD_INTERNAL_REFERENCE = 0x08;
static constexpr uint8_t DAC_CHANNEL_A = 0x00;

void DA4::begin() {
    pinMode(PIN_DA4_CS, OUTPUT);
    digitalWrite(PIN_DA4_CS, HIGH);

    enableInternalReference();
    delay(10);

    writeChannelA(2048);
}

void DA4::writeCommand(uint32_t word) {
    radioSPI.beginTransaction(
        SPISettings(DA4_SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0)
    );

    digitalWrite(PIN_DA4_CS, LOW);
    radioSPI.transfer32(word);
    digitalWrite(PIN_DA4_CS, HIGH);

    radioSPI.endTransaction();
}

void DA4::enableInternalReference() {
    writeCommand(
        (static_cast<uint32_t>(CMD_INTERNAL_REFERENCE) << 24) |
        0x00000001UL
    );
}

void DA4::writeChannelA(uint16_t code) {
    code &= 0x0FFF;

    const uint32_t word =
        (static_cast<uint32_t>(CMD_WRITE_AND_UPDATE) << 24) |
        (static_cast<uint32_t>(
            DAC_CHANNEL_A | ((code >> 8) & 0x0F)
        ) << 16) |
        (static_cast<uint32_t>(code & 0xFF) << 8);

    writeCommand(word);
}