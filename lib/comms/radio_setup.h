#ifndef RADIO_SETUP_H
#define RADIO_SETUP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declaration of the RadioLib type
typedef struct RadioLibSX127x RadioLibSX127x_t;

// Initialize the radio hardware and attach the interrupt callback
bool radio_setup_init(void (*interrupt_callback)(void));

// Get the underlying radio instance (useful for blocking OTA reflashing)
RadioLibSX127x_t* radio_get_instance(void);

// High-level API to hide RadioLib complexity
void radio_start_receive(void);
void radio_start_transmit(const uint8_t *data, size_t len);
void radio_finish_transmit(void);
int16_t radio_read_data(uint8_t *buffer, size_t len);

#endif // RADIO_SETUP_H