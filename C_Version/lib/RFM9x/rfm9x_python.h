#ifndef RFM9X_PYTHON_H
#define RFM9X_PYTHON_H

#include "pico/stdlib.h"
#include "hardware/spi.h"

// RadioHead compatibility constants
#define RH_BROADCAST_ADDRESS 0xFF

typedef struct {
    spi_inst_t *spi;
    uint ss_pin;
    uint reset_pin;
    float frequency;
    
    // RadioHead Headers (matching Python attributes)
    uint8_t node;           // Default: 0xFF
    uint8_t destination;    // Default: 0xFF
    uint8_t identifier;     // Default: 0
    uint8_t flags;          // Default: 0
    
    // Internal state
    bool crc_enabled;
    int last_rssi;
} rfm9x_t;

// API mimicking Python RFM9x class
bool rfm9x_init(rfm9x_t *rfm, spi_inst_t *spi, uint ss, uint rst, float freq);
bool rfm9x_send(rfm9x_t *rfm, const uint8_t *data, uint8_t len, uint8_t tx_power);
int  rfm9x_receive(rfm9x_t *rfm, uint8_t *buffer, uint8_t max_len, float timeout_s, bool with_header);

// Configuration methods
void rfm9x_set_tx_power(rfm9x_t *rfm, int power);
void rfm9x_set_signal_bandwidth(rfm9x_t *rfm, long bw);
void rfm9x_set_spreading_factor(rfm9x_t *rfm, int sf);

#endif