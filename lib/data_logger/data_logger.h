#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <stdint.h>
#include <stddef.h>

/**
 * Sensor Reading structure compatible with surface telemetry
 */
typedef struct {
  uint16_t company_number;
  uint32_t time_ms;
  float depth_m;
  uint16_t actuator_pos;
} SensorReading_t;

/**
 * Initialize the data logger
 */
void data_logger_init(void);

/**
 * Clear all logged samples and free memory
 */
void data_logger_reset(void);

/**
 * Add a sample to the log (automatically reallocates memory if needed)
 * @param co: Company ID
 * @param time: Timestamp in ms
 * @param depth: Depth in meters
 * @param actuator_pos: Actuator position (ADC value)
 */
void data_logger_add_sample(uint16_t co, uint32_t time, float depth, uint16_t actuator_pos);

/**
 * Dump all logged data as CSV formatted text to stdout
 */
void data_logger_dump_csv(void);

/**
 * Get total number of logged samples
 */
size_t data_logger_get_count(void);

#endif // DATA_LOGGER_H
