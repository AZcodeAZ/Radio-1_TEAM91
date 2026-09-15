#include "mcp3208.h"
#include "board_pins.h"
#include <SPI.h>

extern SPIClass radioSPI;

// Conservative clock rate for initial MCP3208 bring-up.
// The MCP3208 maximum clock depends on supply voltage and conditions;
// 1 MHz is a safe debugging starting point.
static constexpr uint32_t MCP3208_SPI_HZ = 1000000;

void MCP3208::begin() {
    pinMode(PIN_MCP3208_CS, OUTPUT);
    digitalWrite(PIN_MCP3208_CS, HIGH);
}

uint16_t MCP3208::readChannel(uint8_t channel) {
    channel &= 0x07;

    // MCP3208 command:
    // Byte 1: 0000011D2
    // Byte 2: D1D0xxxxxx
    // Byte 3: dummy clocks to shift out the lower eight result bits.
    const uint8_t command1 = 0x06 | (channel >> 2);
    const uint8_t command2 = (channel & 0x03) << 6;

    radioSPI.beginTransaction(
        SPISettings(MCP3208_SPI_HZ, MSBFIRST, SPI_MODE0)
    );

    digitalWrite(PIN_MCP3208_CS, LOW);

    radioSPI.transfer(command1);
    const uint8_t highByte = radioSPI.transfer(command2);
    const uint8_t lowByte = radioSPI.transfer(0x00);

    digitalWrite(PIN_MCP3208_CS, HIGH);
    radioSPI.endTransaction();

    return (static_cast<uint16_t>(highByte & 0x0F) << 8) | lowByte;
}