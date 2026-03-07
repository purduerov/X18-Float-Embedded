#include "storage.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

// Fallback just in case CMake does not define the board flash size automatically
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

// Define the global struct here
float_settings_t current_settings;

void storage_save(void) {
    printf("[STORAGE] Writing settings to Flash...\n");
    
    // Flash programming must be done in page sizes (256 bytes)
    uint8_t buffer[FLASH_PAGE_SIZE];
    memset(buffer, 0, FLASH_PAGE_SIZE);
    memcpy(buffer, &current_settings, sizeof(float_settings_t));

    // Disable interrupts to prevent the CPU from trying to read code 
    // from flash while we are erasing/writing to it.
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
    
    printf("[STORAGE] Flash Save Complete.\n");
}

void storage_load(void) {
    // XIP_BASE is the memory address where Flash is mirrored
    const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    memcpy(&current_settings, flash_target_contents, sizeof(float_settings_t));

    // Check if we have valid data by looking for the magic number
    if (current_settings.magic_number != SETTINGS_MAGIC) {
        printf("[STORAGE] No saved settings found. Initializing defaults.\n");
        current_settings.kp = 1.0f;
        current_settings.ki = 0.5f;
        current_settings.kd = 0.1f;
        current_settings.company_number = 9999;
        current_settings.magic_number = SETTINGS_MAGIC;
        storage_save(); // Save defaults immediately
    } else {
        printf("[STORAGE] Successfully loaded settings from Flash.\n");
        printf("[STORAGE] Loaded PID: P=%.2f, I=%.2f, D=%.2f | Team: %u\n", 
               current_settings.kp, current_settings.ki, current_settings.kd, current_settings.company_number);
    }
}

void storage_init(void) {
    storage_load();
}