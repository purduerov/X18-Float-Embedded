#ifndef HW_CONFIG_H
#define HW_CONFIG_H

#include "hardware/i2c.h"

// ==========================================
// 1. HARDWARE PINOUTS
// ==========================================

// --- I2C / Sensor Setup ---
#define I2C_PORT i2c1
#define PIN_SDA 2
#define PIN_SCL 3
#define I2C_BAUDRATE (10 * 1000) //  10 kHz for better noise immunity

// --- Actuator Pins ---
#define PIN_POT 26
#define PIN_EXT 12
#define PIN_RET 13
#define PIN_VREF 27

// --- Radio (LoRa SPI) Pins ---
#define PIN_SPI_MOSI 19
#define PIN_SPI_MISO 20
#define PIN_SPI_SCK 18
#define PIN_CS 24
#define PIN_RST 25
#define PIN_EN 8
#define PIN_IRQ 9

// ==========================================
// 2. SYSTEM PARAMETERS
// ==========================================

// --- Radio Configuration ---
#define RADIO_FREQ 915.0f    // MHz
#define RADIO_BW 7.8f        // kHz
#define RADIO_SF 12           // Spreading Factor
#define RADIO_POWER 29       // dBm
#define RADIO_CR 8           // Coding Rate (4/n)
#define RADIO_SYNC_WORD 0x12 // LoRa Sync Word
#define SPI_BAUDRATE 8000000 // 8 MHz

// --- Actuator Parameters ---
#define DEFAULT_ACTUATOR_POS 4095 // Default to fully extended
#define ACT_POS_TOL 50            // ADC units (~2% of 4095)
#define ACT_ADC_MAX 4095          // 12-bit ADC max
#define ACT_MOVE_TIMEOUT_MS 5000  // Max time to reach target

// --- PID Defaults (If not synced) ---
#define DEFAULT_PID_P 3.2f
#define DEFAULT_PID_I 4.5f
#define DEFAULT_PID_D 38.4f

// --- Mission Defaults ---
#define DEFAULT_MISSION_DUR_S 40
#define DEFAULT_COMPANY_ID 67

#endif // HW_CONFIG_H
