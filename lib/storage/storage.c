#include "storage.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hw_config.h"
#include "sw_config.h"
#include "crc32.h"

// Fallback just in case CMake does not define the board flash size automatically
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif

#define FLASH_OFFSET_A (PICO_FLASH_SIZE_BYTES - 2 * FLASH_SECTOR_SIZE)
#define FLASH_OFFSET_B (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

// Catch at compile time if the settings struct grows beyond one flash page (256 bytes).
// flash_range_program() would silently truncate a write larger than FLASH_PAGE_SIZE.
_Static_assert(sizeof(float_settings_t) <= FLASH_PAGE_SIZE,
    "float_settings_t exceeds FLASH_PAGE_SIZE (256 bytes) — increase buffer or shrink struct");

// Define the global struct here
static float_settings_t current_settings;

static uint32_t calculate_settings_crc(const float_settings_t *settings) {
    // Calculate CRC of all bytes up to but not including the crc field
    size_t crc_len = offsetof(float_settings_t, crc);
    return crc32_software((const uint8_t *)settings, crc_len, 0xFFFFFFFF);
}

static bool is_settings_valid(const float_settings_t *settings) {
    if (settings->magic_number != SETTINGS_MAGIC) {
        return false;
    }
    return calculate_settings_crc(settings) == settings->crc;
}

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
    printf("[STORAGE] Writing settings to Flash (Ping-Pong)...\n");
    
    // Read existing sectors to find the highest sequence number
    const float_settings_t *sector_a = (const float_settings_t *)(XIP_BASE + FLASH_OFFSET_A);
    const float_settings_t *sector_b = (const float_settings_t *)(XIP_BASE + FLASH_OFFSET_B);
    
    uint32_t next_seq = 0;
    uint32_t target_offset = FLASH_OFFSET_A; // Default to sector A
    
    bool a_valid = is_settings_valid(sector_a);
    bool b_valid = is_settings_valid(sector_b);
    
    if (a_valid && b_valid) {
        if (sector_a->sequence_number >= sector_b->sequence_number) {
            next_seq = sector_a->sequence_number + 1;
            target_offset = FLASH_OFFSET_B; // Overwrite oldest (Sector B)
        } else {
            next_seq = sector_b->sequence_number + 1;
            target_offset = FLASH_OFFSET_A; // Overwrite oldest (Sector A)
        }
    } else if (a_valid) {
        next_seq = sector_a->sequence_number + 1;
        target_offset = FLASH_OFFSET_B; // Overwrite corrupt Sector B
    } else if (b_valid) {
        next_seq = sector_b->sequence_number + 1;
        target_offset = FLASH_OFFSET_A; // Overwrite corrupt Sector A
    } else {
        next_seq = 1;
        target_offset = FLASH_OFFSET_A; // Both corrupt or factory clean
    }
    
    current_settings.sequence_number = next_seq;
    current_settings.crc = calculate_settings_crc(&current_settings);
    
    uint8_t buffer[FLASH_PAGE_SIZE];
    memset(buffer, 0, FLASH_PAGE_SIZE);
    memcpy(buffer, &current_settings, sizeof(float_settings_t));
    
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(target_offset, FLASH_SECTOR_SIZE);
    flash_range_program(target_offset, buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
    
    printf("[STORAGE] Flash Saved to sector %s (Seq=%lu)\n", 
           (target_offset == FLASH_OFFSET_A) ? "A" : "B", (unsigned long)next_seq);
}

static void storage_load(void) {
    const float_settings_t *sector_a = (const float_settings_t *)(XIP_BASE + FLASH_OFFSET_A);
    const float_settings_t *sector_b = (const float_settings_t *)(XIP_BASE + FLASH_OFFSET_B);
    
    bool a_valid = is_settings_valid(sector_a);
    bool b_valid = is_settings_valid(sector_b);
    
    const float_settings_t *newest = NULL;
    
    if (a_valid && b_valid) {
        if (sector_a->sequence_number >= sector_b->sequence_number) {
            newest = sector_a;
        } else {
            newest = sector_b;
        }
    } else if (a_valid) {
        newest = sector_a;
    } else if (b_valid) {
        newest = sector_b;
    }
    
    if (newest != NULL) {
        memcpy(&current_settings, newest, sizeof(float_settings_t));
        printf("[STORAGE] Successfully loaded settings from Flash sector %s (Seq=%lu).\n",
               (newest == sector_a) ? "A" : "B", (unsigned long)newest->sequence_number);
        printf("[STORAGE] PID: P=%.2f, I=%.2f, D=%.2f | Deep: %.2fm | Shallow: %.2fm | N: %u | Team: %u | Time: %us | Neutral: %u\n", 
               current_settings.kp, current_settings.ki, current_settings.kd, 
               current_settings.deep_target_m, current_settings.shallow_target_m,
               current_settings.num_profiles,
               current_settings.company_number, current_settings.profile_duration_s,
               current_settings.neutral_buoyancy_adc);
    } else {
        printf("[STORAGE] No valid saved settings found. Initializing defaults.\n");
        current_settings.kp = DEFAULT_PID_P;
        current_settings.ki = DEFAULT_PID_I;
        current_settings.kd = DEFAULT_PID_D;
        current_settings.deep_target_m = 2.5f;
        current_settings.shallow_target_m = 0.4f;
        current_settings.num_profiles = 2;
        current_settings.company_number = 18;
        current_settings.profile_duration_s = 30; // MATE req: 30s
        current_settings.depth_offset = 0.0f; // Must be explicitly zeroed before dive
        current_settings.act_min = 200; // Safe mechanical limit
        current_settings.act_max = 3900; // Safe mechanical limit
        current_settings.neutral_buoyancy_adc = DEFAULT_NEUTRAL_ADC;
        current_settings.arrival_band_m = ARRIVAL_BAND_M;
        current_settings.magic_number = SETTINGS_MAGIC;
        current_settings.sequence_number = 0;
        current_settings.crc = 0;
        storage_save(); 
    }
}

void storage_init(void) {
    storage_load();
}