#ifndef HW_INIT_H
#define HW_INIT_H

#include "hardware/i2c.h"
#include "ms5837.h"
#include <stdbool.h>

/**
 * @brief Initializes the I2C port and GPIO pins based on hw_config.h
 */
void hw_init_i2c(void);

/**
 * @brief Performs the standard initialization sequence for the MS5837 depth sensor
 * 
 * @param sensor Pointer to the MS5837 sensor structure to initialize
 * @return true if initialization succeeded, false otherwise
 */
bool hw_init_depth_sensor(MS5837_t *sensor);

#endif // HW_INIT_H
