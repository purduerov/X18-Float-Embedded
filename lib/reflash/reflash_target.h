#ifndef REFLASH_TARGET_H
#define REFLASH_TARGET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "radiolib_sx1276.h"

// Check if an incoming packet is an OTA reflash packet, and process it entirely if so.
// Returns true if the packet was consumed by the reflash routine.
bool reflash_target_process_packet(RadioLibSX127x_t *lora, uint8_t *packet, size_t len);

#endif // REFLASH_TARGET_H