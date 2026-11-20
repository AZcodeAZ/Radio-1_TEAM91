#include "filter_control.h"
#include "mux_logic.h"
#include "adc_control.h"

volatile uint8_t g_currentAdcRxChannel = 0; // Default: ADC CH0 (NOTCH)
volatile uint8_t g_currentPcbTxChannel = 0; // Default: MUX CH0 (HP 120Hz)

void filterControlInit() {
    muxInit();
    // Default initial states: Set RX to NOTCH (ADC CH0) and TX to HP 120Hz (PCB MUX CH0)
    g_currentAdcRxChannel = 0;
    selectTxFilter(0); 
}

bool processFilterCommandSequence(uint8_t byte1_target, uint8_t byte0_filter) {
    if (byte1_target == CMD_TARGET_RX) {
        switch (byte0_filter) {
            case RX_FILTER_NOTCH:
                g_currentAdcRxChannel = 0; // Select MCP3204 CH0
                Serial.println("[FILTER] RX Target -> CH0 (NOTCH)");
                return true;

            case RX_FILTER_LP1_5KHZ:
                g_currentAdcRxChannel = 1; // Select MCP3204 CH1
                Serial.println("[FILTER] RX Target -> CH1 (LP1 5kHz)");
                return true;

            case RX_FILTER_RAW:
                g_currentAdcRxChannel = 2; // Select MCP3204 CH2
                Serial.println("[FILTER] RX Target -> CH2 (RAW_RX)");
                return true;

            default:
                Serial.printf("[ERROR] Invalid RX Filter Byte: 0x%02X\n", byte0_filter);
                return false;
        }
    } 
    else if (byte1_target == CMD_TARGET_TX) {
        switch (byte0_filter) {
            case TX_FILTER_HP_120HZ:
                g_currentPcbTxChannel = 0;
                selectTxFilter(0); // Set PCB MUX Pins to 00
                Serial.println("[FILTER] TX Target -> CH0 (HP 120Hz)");
                return true;

            case TX_FILTER_RAW:
                g_currentPcbTxChannel = 1;
                selectTxFilter(1); // Set PCB MUX Pins to 01
                Serial.println("[FILTER] TX Target -> CH1 (RAW_TX)");
                return true;

            case TX_FILTER_LP1_5KHZ:
                g_currentPcbTxChannel = 2;
                selectTxFilter(2); // Set PCB MUX Pins to 10
                Serial.println("[FILTER] TX Target -> CH2 (LP1 5kHz)");
                return true;

            default:
                Serial.printf("[ERROR] Invalid TX Filter Byte: 0x%02X\n", byte0_filter);
                return false;
        }
    }

    Serial.printf("[ERROR] Invalid Target Byte: 0x%02X\n", byte1_target);
    return false;
}
