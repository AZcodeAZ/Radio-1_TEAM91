#include <Arduino.h>
#include <SPI.h>
#include <atomic>
#include "board_pins.h"
#include "mux_logic.h"
#include "dac_control.h"
#include "adc_control.h"

// Hardware SPI Buses
#if CONFIG_IDF_TARGET_ESP32H2
SPIClass spiADC(FSPI); // ESP32-H2 only has FSPI (no HSPI peripheral)
SPIClass spiDAC(FSPI); // Satisfies extern SPIClass spiDAC in dac_control.cpp
constexpr int SWITCH_PIN = PIN_RF_SWITCH;
constexpr uint8_t ONBOARD_LED_PIN = 8;  // ESP32-H2 onboard WS2812 RGB LED
#else
SPIClass spiADC(FSPI); // S3 Hardware SPI2 (ADC)
SPIClass spiDAC(HSPI); // S3 Hardware SPI3 (DAC)
constexpr int SWITCH_PIN = PIN_RF_SWITCH;
constexpr uint8_t ONBOARD_LED_PIN = 38; // ESP32-S3 onboard WS2812 RGB LED
#endif

// Audio & Battery Timing
constexpr unsigned long TX_TIMEOUT_MS = 200;
std::atomic<uint32_t> lastTxDataMs{0};

// Sample Rate Diagnostics (written by Core 1 audio loop, printed by Core 0 USB task)
std::atomic<uint32_t> measuredRate{0};  // Samples in the Last ~1 s Window (0 = No New Value)
std::atomic<uint32_t> overrunCount{0};  // Times the Loop Fell >2 ms Behind and Reset Its Clock

// Battery State & Debounce
bool debouncedLowState = false;
static unsigned long lastBatteryCheckMs = 0;
static uint8_t lowCounter = 0;
static uint8_t highCounter = 0;
constexpr uint8_t STABILITY_THRESHOLD = 5;
std::atomic<int8_t> pendingBatteryStatus{-1}; // -1 = none, 0 = normal, 1 = low

// Shared Filter & Mode State (written by Core 0 USB task, read by Core 1 audio loop)
std::atomic<uint8_t> rxFilterChannel{2};    // Default: ADC CH2 (RAW_RX)
std::atomic<uint8_t> txFilterChannel{1};    // Default: PCB MUX CH1 (RAW_TX)
std::atomic<bool>    txEnabled{true};       // Master TX gate (toggled via 0xD1/0xD2)
std::atomic<bool>    amDemodEnabled{false}; // false = wire loopback; true = RF superheterodyne IF

// Core 1 Local State
bool isTxActive = false;
uint8_t appliedTxFilter = 255;
uint16_t lastDacSample = 2048;

// Serial Protocol Command Definitions
constexpr uint8_t CMD_TARGET_TX_GATE = 0xD0; // TX gate & AM demodulator control
constexpr uint8_t CMD_TARGET_RX      = 0xE0; // RX Filter selection
constexpr uint8_t CMD_TARGET_TX      = 0xF0; // TX Filter selection

// DAC Sample Packet (PC -> ESP32): 0xAA 0x55 | Count (uint16 LE) | Count x Sample (uint16 LE 0..4095)
constexpr uint8_t TX_SYNC_0 = 0xAA;
constexpr uint8_t TX_SYNC_1 = 0x55;
constexpr uint16_t TX_MAX_PACKET_SAMPLES = 480;

// ADC Sample Packet (ESP32 -> PC): 0xAA 0xBB | Count (uint16 LE) | Count x Sample (uint16 LE 0..4095)
constexpr uint8_t RX_SYNC_0 = 0xAA;
constexpr uint8_t RX_SYNC_1 = 0xBB;
constexpr uint16_t RX_PACKET_SAMPLES = 128; // 260 bytes/packet = 97.5 KB/s at 48 kHz

// Lock-Free Ring Buffers Between Core 1 (48 kHz SPI) and Core 0 (USB CDC Bulk I/O)
constexpr uint32_t DAC_RING_SIZE = 2048;       // Power of 2 (~42 ms max TX latency)
constexpr uint32_t DAC_RING_MASK = DAC_RING_SIZE - 1;
constexpr uint32_t DAC_PREFILL_SAMPLES = 960;  // 20 ms jitter cushion before DAC starts

constexpr uint32_t RX_RING_SIZE = 16384;       // Power of 2 (~341 ms RX USB cushion)
constexpr uint32_t RX_RING_MASK = RX_RING_SIZE - 1;

uint16_t dacBuffer[DAC_RING_SIZE];
std::atomic<uint32_t> dacHead{0};
std::atomic<uint32_t> dacTail{0};
bool dacPlaying = false;

uint16_t rxBuffer[RX_RING_SIZE];
std::atomic<uint32_t> rxHead{0};
std::atomic<uint32_t> rxTail{0};

// AM Envelope Detector Gain (boosts 0-1V RF swing into full 3.3V UI/DAC range)
constexpr float AM_AUDIO_GAIN = 2.5f;

// Serial Parser State (owned exclusively by Core 0)
enum class RxState : uint8_t { Idle, CommandCode, Sync, LengthLo, LengthHi, SampleLo, SampleHi };
RxState rxState = RxState::Idle;
uint8_t commandTarget = 0;
uint16_t samplesRemaining = 0;
uint8_t sampleLo = 0;

// 48 kHz Firmware AM Envelope Detector
uint16_t demodulateAmEnvelope(uint16_t rawAdc) {
    static float inputDc = 2048.0f;
    static float lp1 = 0.0f, lp2 = 0.0f, lp3 = 0.0f;
    static float envDc = 0.0f;

    float x = (float)rawAdc;
    inputDc += 0.002f * (x - inputDc);
    float acIf = x - inputDc;

    float rect = fabsf(acIf);

    constexpr float ALPHA_LP = 0.65f;
    lp1 += ALPHA_LP * (rect - lp1);
    lp2 += ALPHA_LP * (lp1 - lp2);
    lp3 += ALPHA_LP * (lp2 - lp3);

    envDc += 0.002f * (lp3 - envDc);
    float audioOut = 2048.0f + (lp3 - envDc) * AM_AUDIO_GAIN;

    if (audioOut < 0.0f) return 0;
    if (audioOut > 4095.0f) return 4095;
    return (uint16_t)audioOut;
}

void resetDacPlayback() {
    dacTail.store(dacHead.load(std::memory_order_acquire), std::memory_order_release);
    dacPlaying = false;
    lastDacSample = 2048;
    dacWrite(2048); // Return DAC to mid-scale silence (1.65V -> ~0.5V after divider)
}

// Update TX Chain & Onboard LED on Core 1
void setTxActive(bool txOn) {
    if (txOn == isTxActive) return;
    isTxActive = txOn;

    if (isTxActive) {
        // NOTE: If PIN_RF_SWITCH disconnects RX when HIGH, keep this LOW for full-duplex
        digitalWrite(SWITCH_PIN, HIGH);
        setTxMuxEnable(true);
        appliedTxFilter = txFilterChannel.load(std::memory_order_relaxed);
        selectTxFilter(appliedTxFilter);
        neopixelWrite(ONBOARD_LED_PIN, 10, 10, 10); // WHITE: Full-duplex (RX + TX)
    } else {
        digitalWrite(SWITCH_PIN, LOW);
        setTxMuxEnable(false);
        resetDacPlayback();
        neopixelWrite(ONBOARD_LED_PIN, 0, 10, 0);   // GREEN: RX only (TX idle)
    }
}

// Command Handler (runs on Core 0)
void handleControlCommand(uint8_t target, uint8_t code) {
    if (target == CMD_TARGET_TX_GATE) {
        switch (code) {
            case 0xD1: txEnabled.store(true, std::memory_order_release); break;
            case 0xD2: txEnabled.store(false, std::memory_order_release); break;
            case 0xD3: amDemodEnabled.store(true, std::memory_order_release); break;
            case 0xD4: amDemodEnabled.store(false, std::memory_order_release); break;
            default: break;
        }
    } else if (target == CMD_TARGET_RX) {
        switch (code) {
            case 0xE1: rxFilterChannel.store(0, std::memory_order_release); break; // CH0 - NOTCH
            case 0xE2: rxFilterChannel.store(1, std::memory_order_release); break; // CH1 - LP1 (5kHz)
            case 0xE3: rxFilterChannel.store(2, std::memory_order_release); break; // CH2 - RAW_RX
            default: break;
        }
    } else if (target == CMD_TARGET_TX) {
        switch (code) {
            case 0xF1: txFilterChannel.store(0, std::memory_order_release); break; // CH0 - HP (120 Hz)
            case 0xF2: txFilterChannel.store(1, std::memory_order_release); break; // CH1 - RAW_TX
            case 0xF3: txFilterChannel.store(2, std::memory_order_release); break; // CH2 - LP1 (5kHz)
            default: break;
        }
    }
}

void pushDacSample(uint16_t sample) {
    uint32_t head = dacHead.load(std::memory_order_relaxed);
    uint32_t tail = dacTail.load(std::memory_order_acquire);
    if (head - tail < DAC_RING_SIZE) {
        dacBuffer[head & DAC_RING_MASK] = sample;
        dacHead.store(head + 1, std::memory_order_release);
    }
}

void parseSerialByte(uint8_t b, uint32_t nowMs) {
    switch (rxState) {
        case RxState::Idle:
            if (b == CMD_TARGET_TX_GATE || b == CMD_TARGET_RX || b == CMD_TARGET_TX) {
                commandTarget = b;
                rxState = RxState::CommandCode;
            } else if (b == TX_SYNC_0) {
                rxState = RxState::Sync;
            }
            break;

        case RxState::CommandCode:
            handleControlCommand(commandTarget, b);
            rxState = RxState::Idle;
            break;

        case RxState::Sync:
            rxState = (b == TX_SYNC_1) ? RxState::LengthLo : RxState::Idle;
            break;

        case RxState::LengthLo:
            samplesRemaining = b;
            rxState = RxState::LengthHi;
            break;

        case RxState::LengthHi:
            samplesRemaining |= (uint16_t)b << 8;
            if (samplesRemaining == 0 || samplesRemaining > TX_MAX_PACKET_SAMPLES) {
                rxState = RxState::Idle;
                break;
            }
            if (txEnabled.load(std::memory_order_relaxed)) {
                lastTxDataMs.store(nowMs, std::memory_order_release);
            }
            rxState = RxState::SampleLo;
            break;

        case RxState::SampleLo:
            sampleLo = b;
            rxState = RxState::SampleHi;
            break;

        case RxState::SampleHi: {
            if ((b & 0xF0) != 0) {
                rxState = RxState::Idle;
                break;
            }
            if (txEnabled.load(std::memory_order_relaxed)) {
                uint16_t dacSample = ((uint16_t)b << 8) | sampleLo;
                pushDacSample(dacSample);
            }
            if (--samplesRemaining == 0) {
                if (txEnabled.load(std::memory_order_relaxed)) {
                    lastTxDataMs.store(nowMs, std::memory_order_release);
                }
                rxState = RxState::Idle;
            } else {
                rxState = RxState::SampleLo;
            }
            break;
        }
    }
}

void checkBatteryStatus(unsigned long currentMillis) {
    if (currentMillis - lastBatteryCheckMs >= 50) {
        lastBatteryCheckMs = currentMillis;

        if (digitalRead(PIN_BATTERY_SENSE) == LOW) {
            highCounter = 0;
            if (lowCounter < STABILITY_THRESHOLD) lowCounter++;
        } else {
            lowCounter = 0;
            if (highCounter < STABILITY_THRESHOLD) highCounter++;
        }

        if (lowCounter >= STABILITY_THRESHOLD && !debouncedLowState) {
            debouncedLowState = true;
            pendingBatteryStatus.store(1, std::memory_order_release);
        } else if (highCounter >= STABILITY_THRESHOLD && debouncedLowState) {
            debouncedLowState = false;
            pendingBatteryStatus.store(0, std::memory_order_release);
        }
    }
}

// Write all bytes of a framed packet to TinyUSB CDC without partial-write truncation
void serialWriteExact(const uint8_t* data, size_t len) {
    size_t sent = 0;
    uint32_t startMs = millis();
    while (sent < len) {
        size_t n = Serial.write(data + sent, len - sent);
        if (n > 0) {
            sent += n;
        } else {
            if (millis() - startMs > 50) {
                break;
            }
            vTaskDelay(1);
        }
    }
}

// Core 0 Dedicated USB CDC Task: non-blocking bulk RX + complete 260-byte TX packets
void usbSerialTask(void* param) {
    static uint8_t usbInBuf[2048];
    static uint8_t usbOutBuf[4 + RX_PACKET_SAMPLES * 2]; // 260 bytes per 128 samples

    usbOutBuf[0] = RX_SYNC_0;
    usbOutBuf[1] = RX_SYNC_1;
    usbOutBuf[2] = (uint8_t)(RX_PACKET_SAMPLES & 0xFF);
    usbOutBuf[3] = (uint8_t)((RX_PACKET_SAMPLES >> 8) & 0xFF);

    for (;;) {
        bool didWork = false;

        // 1. Non-blocking bulk drain of incoming USB RX bytes (TX audio + MUX/Demod commands)
        int availIn = Serial.available();
        if (availIn > 0) {
            size_t toRead = (availIn > (int)sizeof(usbInBuf)) ? sizeof(usbInBuf) : (size_t)availIn;
            size_t nRead = Serial.read(usbInBuf, toRead);
            uint32_t nowMs = millis();
            for (size_t i = 0; i < nRead; i++) {
                parseSerialByte(usbInBuf[i], nowMs);
            }
            didWork = true;
        }

        // 2. Send battery frame if state changed (always outside audio packets)
        int8_t bat = pendingBatteryStatus.exchange(-1, std::memory_order_acq_rel);
        if (bat >= 0) {
            uint8_t frame[2] = { (uint8_t)bat, 0xCC };
            serialWriteExact(frame, 2);
            didWork = true;
        }

        // 2b. Report the measured sample rate once per second (between packets, so frames stay intact)
        uint32_t rate = measuredRate.exchange(0, std::memory_order_acq_rel);
        if (rate > 0) {
            Serial.printf("[RATE] %u Hz, %u overruns\n", (unsigned)rate,
                          (unsigned)overrunCount.load(std::memory_order_relaxed));
            didWork = true;
        }

        // 3. Bulk-transmit 128 RX samples in a complete, untruncated 260-byte packet
        uint32_t head = rxHead.load(std::memory_order_acquire);
        uint32_t tail = rxTail.load(std::memory_order_relaxed);
        while (head - tail >= RX_PACKET_SAMPLES) {
            for (uint32_t i = 0; i < RX_PACKET_SAMPLES; i++) {
                uint16_t s = rxBuffer[(tail + i) & RX_RING_MASK];
                uint32_t idx = 4 + i * 2;
                usbOutBuf[idx]     = (uint8_t)(s & 0xFF);
                usbOutBuf[idx + 1] = (uint8_t)((s >> 8) & 0x0F);
            }
            serialWriteExact(usbOutBuf, sizeof(usbOutBuf));
            tail += RX_PACKET_SAMPLES;
            rxTail.store(tail, std::memory_order_release);
            didWork = true;
        }

        if (!didWork) {
            vTaskDelay(1);
        }
    }
}

void setup() {
    Serial.setRxBufferSize(8192);
    Serial.begin(921600);

    unsigned long start = millis();
    while (!Serial && (millis() - start < 3000)) {
        delay(10);
    }

    pinMode(SWITCH_PIN, OUTPUT);
    pinMode(ONBOARD_LED_PIN, OUTPUT);

#if CONFIG_IDF_TARGET_ESP32S3
    gpio_reset_pin((gpio_num_t)PIN_ADC_SCK);
    gpio_reset_pin((gpio_num_t)PIN_ADC_MISO);
#endif

    pinMode(PIN_CS_ADC, OUTPUT);
    digitalWrite(PIN_CS_ADC, HIGH);

    pinMode(PIN_CS_DAC, OUTPUT);
    digitalWrite(PIN_CS_DAC, HIGH);

    pinMode(PIN_BATTERY_SENSE, INPUT_PULLUP);

    spiADC.begin(PIN_ADC_SCK, PIN_ADC_MISO, PIN_ADC_MOSI, PIN_CS_ADC);
#if !CONFIG_IDF_TARGET_ESP32H2
    spiDAC.begin(PIN_DAC_SCK, -1, PIN_DAC_MOSI, PIN_CS_DAC);
#endif

    dacBegin();
    muxInit();

    // Default boot state: RX active (Green LED), TX idle until packets arrive
    isTxActive = true;
    setTxActive(false);

    // Initial Telemetry Broadcast
    debouncedLowState = (digitalRead(PIN_BATTERY_SENSE) == LOW);
    pendingBatteryStatus.store(debouncedLowState ? 1 : 0, std::memory_order_release);

#if !CONFIG_FREERTOS_UNICORE
    disableCore1WDT();
    xTaskCreatePinnedToCore(usbSerialTask, "usb_io", 4096, nullptr, 3, nullptr, 0);
#endif
}

// Core 1 Dedicated 48.000 kHz Audio Loop (never returns to Arduino's serialEventRun wrapper)
void loop() {
    int64_t streamStartUs = esp_timer_get_time();
    uint64_t sampleIndex = 0;
    uint8_t housekeepingDivider = 0;
    int64_t rateWindowUs = streamStartUs;
    uint32_t rateCount = 0;

    for (;;) {
        int64_t nowUs = esp_timer_get_time();
        int64_t targetUs = streamStartUs + (int64_t)((sampleIndex * 1000000ULL) / 48000ULL);

        if (nowUs >= targetUs) {
            if (nowUs - targetUs > 2000) {
                overrunCount.fetch_add(1, std::memory_order_relaxed);
                streamStartUs = nowUs;
                sampleIndex = 0;
            }
            sampleIndex++;

            // Count samples actually taken; publish once per second for the USB task to print
            rateCount++;
            if (nowUs - rateWindowUs >= 1000000) {
                measuredRate.store(rateCount, std::memory_order_release);
                rateCount = 0;
                rateWindowUs = nowUs;
            }

            // Periodic Housekeeping on Core 1 (every 48 samples = ~1 ms)
            if (++housekeepingDivider >= 48) {
                housekeepingDivider = 0;
                uint32_t nowMs = millis();
                checkBatteryStatus(nowMs);

                bool wantTx = txEnabled.load(std::memory_order_acquire) &&
                              ((nowMs - lastTxDataMs.load(std::memory_order_acquire)) < TX_TIMEOUT_MS);
                setTxActive(wantTx);

                if (isTxActive) {
                    uint8_t desiredFilter = txFilterChannel.load(std::memory_order_relaxed);
                    if (desiredFilter != appliedTxFilter) {
                        appliedTxFilter = desiredFilter;
                        selectTxFilter(appliedTxFilter);
                    }
                }
            }

            // A. TX Chain: Clock out next DAC sample at exact 48.000 kHz
            if (isTxActive) {
                uint32_t head = dacHead.load(std::memory_order_acquire);
                uint32_t tail = dacTail.load(std::memory_order_relaxed);
                if (!dacPlaying && (head - tail >= DAC_PREFILL_SAMPLES)) {
                    dacPlaying = true;
                }
                if (dacPlaying) {
                    if (head != tail) {
                        lastDacSample = dacBuffer[tail & DAC_RING_MASK];
                        dacWrite(lastDacSample);
                        dacTail.store(tail + 1, std::memory_order_release);
                    } else {
                        dacWrite(lastDacSample);
                    }
                }
            }

            // B. RX Chain: Always active — sample ADC and push to Core 0 USB ring buffer
            uint16_t adcVal = adcReadFast(rxFilterChannel.load(std::memory_order_relaxed));
            if (amDemodEnabled.load(std::memory_order_relaxed)) {
                adcVal = demodulateAmEnvelope(adcVal);
            }

            uint32_t rHead = rxHead.load(std::memory_order_relaxed);
            uint32_t rTail = rxTail.load(std::memory_order_acquire);
            if (rHead - rTail < RX_RING_SIZE) {
                rxBuffer[rHead & RX_RING_MASK] = adcVal;
                rxHead.store(rHead + 1, std::memory_order_release);
            }
        }
    }
}
