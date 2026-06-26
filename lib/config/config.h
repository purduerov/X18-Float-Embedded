#ifndef CONFIG_H
#define CONFIG_H

#include "hardware/i2c.h"

// === HARDWARE PINS ===
#define I2C_PORT        i2c1
#define PIN_SDA         2
#define PIN_SCL         3
#define I2C_BAUDRATE    (10 * 1000)

#define PIN_POT         26
#define PIN_EXT         12
#define PIN_RET         13
#define PIN_VREF        27

#define PIN_SPI_SCK     14
#define PIN_SPI_MOSI    15
#define PIN_SPI_MISO    8
#define PIN_CS          24
#define PIN_RST         25
#define PIN_IRQ         9
#define PIN_EN          6

#define PIN_NEOPIXEL    21
#define PIN_NEOPIXEL_PWR 20

// === RADIO ===
#define RADIO_FREQ      915.0f
#define RADIO_BW        125.0f
#define RADIO_SF        7
#define RADIO_POWER     17
#define RADIO_CR        5
#define RADIO_SYNC_WORD 0x12
#define SPI_BAUDRATE    8000000

// === PID DEFAULTS ===
#define DEFAULT_PID_P   120.0f
#define DEFAULT_PID_I   0.5f
#define DEFAULT_PID_D   25.0f
#define DEFAULT_NEUTRAL_ADC 1850

// === TIMING ===
#define DEPTH_PID_LOOP_MS   100
#define ACT_LOOP_MS         20
#define SAMPLE_INTERVAL_MS  1000

// === ACTUATOR ===
#define ACT_ADC_MAX         4095
#define ACT_DEADZONE        30
#define ACT_VREF_PWM_WRAP   65535
#define ACT_VREF_MIN_DUTY   19859
#define ACT_VREF_MAX_DUTY   65535
#define ACT_STALL_MS        800
#define ACT_STALL_THRESHOLD 3
#define ACT_MOVE_TIMEOUT_MS 12000

// === DEPTH PID ===
#define INTEGRAL_GATE_M         0.5f
#define NEUTRAL_ADC_MIN_VALID   1300U
#define NEUTRAL_ADC_MAX_VALID   2500U
#define INTEGRAL_BAND           600.0f
#define DERIV_EMA_ALPHA         0.3f

// === MISSION ===
#define DEEP_TOL_M              0.33f
#define SHALLOW_TOL_M           0.1f
#define SURFACE_DETECTION_M     0.05f
#define ADAPTIVE_HOLD_THRESHOLD_M 0.15f
#define STALL_CHECK_DURATION_MS 15000
#define STALL_DEPTH_THRESHOLD_M 0.01f
#define SAFETY_TIMEOUT_S        1000
#define MAX_SAMPLES_PER_STAGE   50
#define MAX_RECORDED_SAMPLES    (MAX_SAMPLES_PER_STAGE * 2 * 3)

// === SENSOR RECOVERY ===
#define SENSOR_RESET_STRIKES    5
#define I2C_BUS_RESET_STRIKES   10
#define I2C_BUS_RESET_THROTTLE_MS 2000
#define SENSOR_ABORT_STRIKES    50

// === RADIO TIMING ===
#define RADIO_DONE_BROADCAST_MS 3000
#define RADIO_DATA_RETRANSMIT_MS 2000
#define RADIO_TEST_TX_MS        1000

// === FIRMWARE ===
#define FIRMWARE_VERSION        200

#endif // CONFIG_H
