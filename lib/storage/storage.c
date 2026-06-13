#include "storage.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hw_config.h"
#include "sw_config.h"

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
        current_settings.kp = DEFAULT_PID_P;
        current_settings.ki = DEFAULT_PID_I;
        current_settings.kd = DEFAULT_PID_D;
        current_settings.deep_target_m = 2.5f;
        current_settings.shallow_target_m = 0.4f;
        current_settings.num_profiles = 2;
        current_settings.company_number = 18;
        current_settings.profile_duration_s = 30; // MATE req: 30s
        current_settings.depth_offset = 0.0f; // Must be explicitly zeroed before dive
        current_settings.act_min = 120; // Safe mechanical limit
        current_settings.act_max = 3900; // Safe mechanical limit
        current_settings.neutral_buoyancy_adc = DEFAULT_NEUTRAL_ADC;
        current_settings.arrival_band_m = ARRIVAL_BAND_M;
        current_settings.magic_number = SETTINGS_MAGIC;
        storage_save(); 
    } else {
        printf("[STORAGE] Successfully loaded settings from Flash.\n");
        printf("[STORAGE] PID: P=%.2f, I=%.2f, D=%.2f | Deep: %.2fm | Shallow: %.2fm | N: %u | Team: %u | Time: %us | Neutral: %u\n", 
               current_settings.kp, current_settings.ki, current_settings.kd, 
               current_settings.deep_target_m, current_settings.shallow_target_m,
               current_settings.num_profiles,
               current_settings.company_number, current_settings.profile_duration_s,
               current_settings.neutral_buoyancy_adc);
    }
}

void storage_init(void) {
    storage_load();
}