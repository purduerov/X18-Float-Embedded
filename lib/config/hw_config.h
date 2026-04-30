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

#ifdef TARGET_SURFACE
#define PIN_CS 17
#define PIN_RST 16
#define PIN_IRQ 2
#else
#define PIN_CS 24
#define PIN_RST 25
#define PIN_IRQ 9
#endif
#define PIN_EN 8

// --- Status LED ---
#define PIN_NEOPIXEL 16

// ==========================================
// 2. SYSTEM PARAMETERS
// ==========================================

// --- Radio Configuration ---
#define RADIO_FREQ 915.0f    // MHz
#define RADIO_BW 125.0f      // kHz
#define RADIO_SF 7           // Spreading Factor (Normal/Fast)
#define RADIO_POWER 17       // dBm (Max for SX1276 driver)
#define RADIO_CR 5           // Coding Rate (4/5)
#define RADIO_SYNC_WORD 0x12 // LoRa Sync Word
#define SPI_BAUDRATE 8000000 // 8 MHz

// --- Actuator Parameters ---
#define DEFAULT_ACTUATOR_POS 4095 // Default to fully extended
#define DEFAULT_NEUTRAL_ADC 2048  // Default mid-point
#define ACT_POS_TOL 20            // ADC units (~2% of 4095)
#define ACT_ADC_MAX 4095          // 12-bit ADC max
#define ACT_MOVE_TIMEOUT_MS 8000  // Max time to reach target
#define ACT_STALL_MS 1000         // Max time without ADC change before stall
#define ACT_RETRY_BACKOFF_MS 2000 // Time to wait before auto-retrying
#define ACT_MAX_RETRIES 1         // Number of allowed retries
#define ACT_FILTER_SIZE 5         // Moving average filter size for ADC
#define ACT_STALL_THRESHOLD 2     // Minimum ADC change to reset stall timer

// --- Actuator Inner PID ---
#define ACT_KP 0.8f
#define ACT_KI 0.05f
#define ACT_KD 0.15f
#define ACT_LOOP_MS 20 // 50Hz control loop
#define ACT_PID_LIMIT 500.0f

// --- Actuator VREF (PWM Power) ---
#define ACT_VREF_PWM_WRAP 65535
#define ACT_VREF_MIN_DUTY 19859 // ~30% power (minimum to move under load)
#define ACT_VREF_MAX_DUTY 65535 // 100% power

// --- Hysteresis / Deadzone ---
#define ACT_DEADZONE_ENTER 20
#define ACT_DEADZONE_EXIT 50

// --- Control Loop Timings ---
#define DEPTH_PID_LOOP_MS 100 // 10Hz outer loop
#define SURFACE_DEBUG_INTERVAL_MS 2000
#define ARRIVAL_BAND_M 0.33f
#define PROFILING_SAFETY_TIMEOUT_S 60

// --- PID Defaults (If not synced) ---
#define DEFAULT_PID_P 1500.0f
#define DEFAULT_PID_I 10.0f
#define DEFAULT_PID_D 2000.0f

// ==========================================
// 3. SYSTEM SETTINGS
// ==========================================

// --- USB Wait Toggles ---
// Set to 1 for testing (waits for serial), 0 for production (immediate boot)
#define FLOAT_ENABLE_USB_WAIT 0
#define SURFACE_ENABLE_USB_WAIT 1

#endif // HW_CONFIG_H
