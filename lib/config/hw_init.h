#ifndef HW_INIT_H
#define HW_INIT_H

#include "hardware/i2c.h"
#include "ms5837.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Performs unified system initialization based on TARGET_* defines.
 * 
 * @param radio_irq_callback Callback for LoRa IRQ. Can be NULL if radio not needed.
 * @param depth_sensor Optional pointer to depth sensor struct (Float only).
 * @return true if all critical hardware initialized successfully.
 */
bool system_init(void (*radio_irq_callback)(void), MS5837_t *depth_sensor);

/**
 * @brief Initializes the I2C port and GPIO pins based on hw_config.h
 */
void hw_init_i2c(void);

/**
 * @brief Shuts down the I2C port and resets GPIO pins.
 */
void hw_deinit_i2c(void);

/**
 * @brief Performs the standard initialization sequence for the MS5837 depth sensor
 * 
 * @param sensor Pointer to the MS5837 sensor structure to initialize
 * @return true if initialization succeeded, false otherwise
 */
bool hw_init_depth_sensor(MS5837_t *sensor);

/**
 * @brief Waits for a USB connection for a specified duration.
 * 
 * @param enabled If false, the function returns immediately.
 * @param timeout_ms Maximum time to wait in milliseconds.
 */
void hw_wait_for_usb(bool enabled, uint32_t timeout_ms);

#endif // HW_INIT_H
