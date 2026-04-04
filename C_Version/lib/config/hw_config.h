#ifndef HW_CONFIG_H
#define HW_CONFIG_H

#include "hardware/i2c.h"

// --- I2C / Sensor Setup ---
#define I2C_PORT i2c1
#define PIN_SDA 2
#define PIN_SCL 3

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

#endif // HW_CONFIG_H
