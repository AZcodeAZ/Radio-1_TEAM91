#include <Arduino.h>
#include <SPI.h>

#include "board_pins.h"
#include "da4.h"
#include "status_led.h"
#include "mcp3208.h"

SPIClass radioSPI(FSPI);
DA4 da4;
MCP3208 adc;
hw_timer_t *dacTimer = NULL;

static constexpr uint32_t SPI_CLOCK_HZ = 8000000;

static constexpr uint16_t DAC_MIDPOINT = 2048;
static constexpr int32_t DAC_GAIN = 2048; // Scaled for 0-2.5V full span

// Packet format:
// 0xAA 0x55 | uint16_t sample_count, little-endian | PCM16 samples
static constexpr uint8_t SYNC_1 = 0xAA;
static constexpr uint8_t SYNC_2 = 0x55;

static constexpr uint16_t MAX_PACKET_SAMPLES = 160;
static constexpr uint16_t AUDIO_BUFFER_SAMPLES = 2048;

int16_t audioBuffer[AUDIO_BUFFER_SAMPLES];

volatile uint16_t writeIndex = 0;
volatile uint16_t readIndex = 0;

enum class ParseState {
    WAIT_SYNC_1,
    WAIT_SYNC_2,
    READ_COUNT_LO,
    READ_COUNT_HI,
    READ_SAMPLES
};

ParseState parseState = ParseState::WAIT_SYNC_1;

uint16_t samplesExpected = 0;
uint16_t samplesReceived = 0;
uint8_t byteLow = 0;
bool waitingForHighByte = false;

bool pushSample(int16_t sample) {
    const uint16_t nextWrite =
        (writeIndex + 1) % AUDIO_BUFFER_SAMPLES;

    if (nextWrite == readIndex) {
        return false;
    }

    audioBuffer[writeIndex] = sample;
    writeIndex = nextWrite;
    return true;
}

bool popSample(int16_t& sample) {
    if (readIndex == writeIndex) {
        return false;
    }

    sample = audioBuffer[readIndex];
    readIndex = (readIndex + 1) % AUDIO_BUFFER_SAMPLES;
    return true;
}

uint16_t pcmToDac(int16_t sample) {
    const int32_t scaled =
        DAC_MIDPOINT + (static_cast<int32_t>(sample) * DAC_GAIN) / 32768;

    return static_cast<uint16_t>(constrain(scaled, 0, 4095));
}

void processSerialByte(uint8_t value) {
    switch (parseState) {
        case ParseState::WAIT_SYNC_1:
            if (value == SYNC_1) {
                parseState = ParseState::WAIT_SYNC_2;
            }
            break;

        case ParseState::WAIT_SYNC_2:
            parseState = (value == SYNC_2)
                ? ParseState::READ_COUNT_LO
                : ParseState::WAIT_SYNC_1;
            break;

        case ParseState::READ_COUNT_LO:
            samplesExpected = value;
            parseState = ParseState::READ_COUNT_HI;
            break;

        case ParseState::READ_COUNT_HI:
            samplesExpected |= static_cast<uint16_t>(value) << 8;

            if (samplesExpected == 0 || samplesExpected > MAX_PACKET_SAMPLES) {
                parseState = ParseState::WAIT_SYNC_1;
            } else {
                samplesReceived = 0;
                waitingForHighByte = false;
                parseState = ParseState::READ_SAMPLES;
            }
            break;

        case ParseState::READ_SAMPLES:
            if (!waitingForHighByte) {
                byteLow = value;
                waitingForHighByte = true;
            } else {
                const int16_t sample = static_cast<int16_t>(
                    static_cast<uint16_t>(byteLow) |
                    (static_cast<uint16_t>(value) << 8)
                );

                pushSample(sample);

                waitingForHighByte = false;
                samplesReceived++;

                if (samplesReceived >= samplesExpected) {
                    parseState = ParseState::WAIT_SYNC_1;
                }
            }
            break;
    }
}

// 8 kHz Timer ISR for consistent sample clocking
void ARDUINO_ISR_ATTR onDacTimer() {
    int16_t pcmSample = 0;
    if (popSample(pcmSample)) {
        statusLedSet(RadioMode::TX);
    } else {
        pcmSample = 0; // Default to zero audio output (midscale DC) on buffer empty
        statusLedSet(RadioMode::IDLE);
    }
    da4.writeChannelA(pcmToDac(pcmSample));
}

void setup() {
    Serial.begin(2000000);
    delay(1500);

    radioSPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);
    adc.begin();
    da4.begin();

    statusLedBegin();
    statusLedSet(RadioMode::IDLE);
    da4.writeChannelA(DAC_MIDPOINT);

    Serial.println("READY_BINARY_AUDIO");
    Serial.println("PCM16 mono, 8 kHz, binary packets.");

    // Setup 8 kHz Hardware Timer Interrupt (125us per sample)
    dacTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(dacTimer, &onDacTimer, true);
    timerAlarmWrite(dacTimer, 125, true);
    timerAlarmEnable(dacTimer);
}

void loop() {
    while (Serial.available() > 0) {
        processSerialByte(static_cast<uint8_t>(Serial.read()));
    }
}