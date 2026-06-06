#ifndef REFLASH_TARGET_H
#define REFLASH_TARGET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "radiolib_sx1276.h"

// Check if an incoming packet is an OTA reflash packet, and process it entirely if so.
// Returns true if the packet was consumed by the reflash routine.
bool reflash_target_process_packet(RadioLibSX127x_t *lora, uint8_t *packet, size_t len);

// Call this periodically from the main loop.
// If a transfer is in progress but no packet has arrived for OTA_TIMEOUT_MS,
// this resets the reflash state and restores 125kHz so a new attempt can begin.
void reflash_target_tick(RadioLibSX127x_t *lora);

// Returns true if an OTA reflash is currently in progress.
bool reflash_target_is_in_progress(void);

#endif // REFLASH_TARGET_H
