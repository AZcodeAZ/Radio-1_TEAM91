#include <Arduino.h>
#include <math.h>

#include "board_pins.h"
#include "dac_control.h"
#include "adc_control.h"


// Threshold setup for a 0 - 3.3V signal (midpoint ~ 2048 count / 1.65V)
constexpr uint16_t THRESHOLD_HIGH = 2100; // Upper hysteresis limit
constexpr uint16_t THRESHOLD_LOW  = 1900; // Lower hysteresis limit

bool isHigh = false;
unsigned long lastCrossingMicros = 0;
float currentFrequencyHz = 0.0f;

// ===============================
// Waveform settings
// ===============================

constexpr uint16_t TABLE_SIZE = 256;

// Select waveform:
// SINE
// TRIANGLE
// SQUARE

enum WaveformType
{
    SINE,
    TRIANGLE,
    SQUARE
};

constexpr WaveformType WAVEFORM = SQUARE;

// Actual measured free-running DAC sample rate
constexpr float SAMPLE_RATE = 100000.0f; // MAY NEED TO CHANGE!!!

// Desired output frequency
constexpr float OUTPUT_FREQUENCY = 20000.0f;

// ===============================
// Waveform table
// ===============================

uint16_t waveformTable[TABLE_SIZE];

void generateWaveformTable()
{
    for (uint16_t i = 0; i < TABLE_SIZE; i++)
    {
        float normalized =
            (float)i / TABLE_SIZE;

        float voltage;

        switch (WAVEFORM)
        {
            // ===============================
            // Sine wave
            // ===============================

            case SINE:
            {
                // 1.25 V DC offset
                // 0.5 V peak amplitude = 1 Vpp

                float angle =
                    2.0f * PI * normalized;

                voltage =
                    1.25f + 0.5f * sin(angle);

                break;
            }

            // ===============================
            // Triangle wave
            // ===============================

            case TRIANGLE:
            {
                float triangle;

                if (normalized < 0.5f)
                {
                    triangle =
                        4.0f * normalized - 1.0f;
                }
                else
                {
                    triangle =
                        3.0f - 4.0f * normalized;
                }

                // Convert -1 to +1
                // into 0.75 V to 1.75 V

                voltage =
                    1.25f + 0.5f * triangle;

                break;
            }

            // ===============================
            // Square wave
            // ===============================

            case SQUARE:
            {
                // 50% duty cycle
                //
                // 0.75 V low
                // 1.75 V high
                //
                // 1 Vpp centered at 1.25 V

                if (normalized < 0.5f)
                {
                    voltage = 1.75f;
                }
                else
                {
                    voltage = 0.75f;
                }

                break;
            }
        }

        waveformTable[i] =
            (uint16_t)(
                (voltage / 2.5f) * 4095.0f
            );
    }
}

// ===============================
// DDS phase accumulator
// ===============================

uint32_t phase = 0;
uint32_t phaseIncrement;

// ===============================
// Setup
// ===============================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32 MCP3208 ADC GRAPHING TEST");
    Serial.println("================================");

    // Initialize ADC hardware
    adcBegin();


    // Generate selected waveform
    /*generateWaveformTable();

    // Calculate DDS phase increment

    phaseIncrement =
        (uint32_t)(
            (OUTPUT_FREQUENCY * 4294967296.0)
            / SAMPLE_RATE
        );

    // ===============================
    // Print waveform selection
    // ===============================

    Serial.print("Waveform: ");

    switch (WAVEFORM)
    {
        case SINE:
            Serial.println("SINE");
            break;

        case TRIANGLE:
            Serial.println("TRIANGLE");
            break;

        case SQUARE:
            Serial.println("SQUARE");
            break;
    }

    Serial.print("Output frequency: ");
    Serial.print(OUTPUT_FREQUENCY);
    Serial.println(" Hz");

    Serial.print("Sample rate: ");
    Serial.print(SAMPLE_RATE);
    Serial.println(" samples/sec");

    Serial.print("Phase increment: ");
    Serial.println(phaseIncrement);

    // ===============================
    // Initialize DAC
    // ===============================

    Serial.println("Initializing DAC...");

    dacBegin();

    Serial.println("DAC initialized.");

    // Start continuous SPI transaction
    dacBeginContinuous();

    Serial.println("DAC waveform generation started.");
    */
}

// ===============================
// Main loop
// ===============================

uint32_t generatedSamples = 0;
uint32_t lastMeasurement = 0;

void loop()
{
    uint16_t rawVal = adcRead(0);
    unsigned long now = micros();

    // Rising edge crossing detection with hysteresis
    if (!isHigh && rawVal > THRESHOLD_HIGH)
    {
        isHigh = true;
        if (lastCrossingMicros > 0)
        {
            unsigned long periodMicros = now - lastCrossingMicros;
            if (periodMicros > 0)
            {
                currentFrequencyHz = 1000000.0f / static_cast<float>(periodMicros);
            }
        }
        lastCrossingMicros = now;
    }
    else if (isHigh && rawVal < THRESHOLD_LOW)
    {
        isHigh = false;
    }

    // Teleplot output updated every 50 ms (20 Hz plot refresh rate)
    static unsigned long lastTeleplotPrint = 0;
    if (millis() - lastTeleplotPrint >= 50)
    {
        Serial.printf(">Frequency_Hz:%.1f\n", currentFrequencyHz);
        Serial.printf(">RawADC:%d\n", rawVal);
        lastTeleplotPrint = millis();
    }
    // Use upper 8 bits of phase
    // to select one of 256 table entries
    /*
    uint8_t tableIndex =
        phase >> 24;

    // Send waveform sample to DAC

    dacWriteContinuous(
        waveformTable[tableIndex]
    );

    // Advance DDS phase

    phase += phaseIncrement;

    // Count generated samples

    generatedSamples++;

    // ===============================
    // Sample-rate measurement
    // ===============================

    uint32_t now = millis();

    if (now - lastMeasurement >= 1000)
    {
        Serial.print("Samples/sec: ");
        Serial.println(generatedSamples);

        generatedSamples = 0;
        lastMeasurement = now;
    }
    */
}