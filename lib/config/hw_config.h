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
#define I2C_BAUDRATE (10 * 1000) // 10 kHz for extreme noise immunity

// --- Actuator Pins ---
#define PIN_POT 26  // A0
#define PIN_EXT 12  // D12
#define PIN_RET 13  // D13
#define PIN_VREF 27 // A1

// --- Radio (LoRa SPI) Pins ---

#ifdef TARGET_SURFACE
#define PIN_SPI_INST spi0
#define PIN_SPI_SCK 18
#define PIN_SPI_MOSI 19
#define PIN_SPI_MISO 16
#define PIN_CS 17  // pin 19 on breadboard on radio side
#define PIN_RST 20 // pin 26 on breadboard on radio side
#define PIN_IRQ 2  // pin 4 on non radio side
#define PIN_EN 8   // pin 11 on non radio side
#else
#define PIN_SPI_INST spi1
#define PIN_SPI_SCK 14
#define PIN_SPI_MOSI 15
#define PIN_SPI_MISO 8
#define PIN_CS 24
#define PIN_RST 25 // D25
#define PIN_IRQ 9  // D9
#define PIN_EN 6   // pin 11 on non radio side

#endif

// --- Status LED ---
#define PIN_NEOPIXEL 21
#define PIN_NEOPIXEL_PWR 20
// ==========================================
// 2. RADIO HARDWARE PARAMS
// ==========================================
#define RADIO_FREQ 915.0f    // MHz
#define RADIO_BW 125.0f      // kHz
#define RADIO_SF 7           // Spreading Factor (Normal/Fast)
#define RADIO_POWER 17       // dBm (Max for SX1276 driver)
#define RADIO_CR 5           // Coding Rate (4/5)
#define RADIO_SYNC_WORD 0x12 // LoRa Sync Word
#define SPI_BAUDRATE 8000000 // 8 MHz

#endif // HW_CONFIG_H
