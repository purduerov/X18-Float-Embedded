#ifndef REFLASH_HOST_H
#define REFLASH_HOST_H

#include "radiolib_sx1276.h"

// Intercepts serial input. Reads 'S' format from flash_tool.py and blocks to stream data over LoRa.
// Call this when 'S' is received from the serial port.
void reflash_host_stream_from_serial(RadioLibSX127x_t *lora);

#endif // REFLASH_HOST_H