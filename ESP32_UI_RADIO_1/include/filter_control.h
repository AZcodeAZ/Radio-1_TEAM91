#ifndef FILTER_CONTROL_H
#define FILTER_CONTROL_H

#include <Arduino.h>

// Byte 1: Command Targets
constexpr uint8_t CMD_TARGET_RX = 0xE0;
constexpr uint8_t CMD_TARGET_TX = 0xF0;

// Byte 0: RX Selection Values
constexpr uint8_t RX_FILTER_NOTCH    = 0xE1; // ADC CH0
constexpr uint8_t RX_FILTER_LP1_5KHZ = 0xE2; // ADC CH1
constexpr uint8_t RX_FILTER_RAW      = 0xE3; // ADC CH2

// Byte 0: TX Selection Values
constexpr uint8_t TX_FILTER_HP_120HZ = 0xF1; // PCB MUX CH0
constexpr uint8_t TX_FILTER_RAW      = 0xF2; // PCB MUX CH1
constexpr uint8_t TX_FILTER_LP1_5KHZ = 0xF3; // PCB MUX CH2

// Global active channels
extern volatile uint8_t g_currentAdcRxChannel;
extern volatile uint8_t g_currentPcbTxChannel;

void filterControlInit();
bool processFilterCommandSequence(uint8_t byte1_target, uint8_t byte0_filter);

#endif // FILTER_CONTROL_H