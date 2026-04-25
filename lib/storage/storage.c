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
static float_settings_t current_settings;

void storage_get_settings(float_settings_t *out_settings) {
    if (out_settings) {
        memcpy(out_settings, &current_settings, sizeof(float_settings_t));
    }
}

void storage_set_settings(const float_settings_t *new_settings) {
    if (new_settings) {
        memcpy(&current_settings, new_settings, sizeof(float_settings_t));
    }
}

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
    
    printf("[STORAGE] Flash Saved\n");
}

static void storage_load(void) {
    const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    memcpy(&current_settings, flash_target_contents, sizeof(float_settings_t));

    if (current_settings.magic_number != SETTINGS_MAGIC) {
        printf("[STORAGE] No saved settings found. Initializing defaults.\n");
        current_settings.kp = 3.2f;
        current_settings.ki = 0.5f;
        current_settings.kd = 38.4f;
        current_settings.target_depth = 1.0f;
        current_settings.company_number = 18;
        current_settings.profile_duration_s = 40;
        current_settings.depth_offset = 1000.0f; // Uncalibrated Indicator
        current_settings.act_min = 0;
        current_settings.act_max = 4095;
        current_settings.magic_number = SETTINGS_MAGIC;
        storage_save(); 
    } else {
        printf("[STORAGE] Successfully loaded settings from Flash.\n");
        printf("[STORAGE] PID: P=%.2f, I=%.2f, D=%.2f | Target: %.2fm | Team: %u | Time: %us | Bounds: [%u, %u]\n", 
               current_settings.kp, current_settings.ki, current_settings.kd, 
               current_settings.target_depth,
               current_settings.company_number, current_settings.profile_duration_s,
               current_settings.act_min, current_settings.act_max);
    }
}

void storage_init(void) {
    storage_load();
}