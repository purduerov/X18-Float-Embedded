#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

// Bump magic number whenever struct layout changes to force a clean factory reset on flash.
// Last bumped: added __attribute__((packed)) to eliminate padding.
#define SETTINGS_MAGIC 0x20261338

typedef struct __attribute__((packed)) {
    float kp;
    float ki;
    float kd;
    float deep_target_m;
    float shallow_target_m;
    uint16_t num_profiles;
    uint16_t company_number;
    uint16_t profile_duration_s; 
    float depth_offset;
    uint16_t act_min;
    uint16_t act_max;
    uint16_t neutral_buoyancy_adc;
    float arrival_band_m;
    uint32_t magic_number; 
} float_settings_t;

// Function prototypes for accessing and modifying settings
void storage_init(void);
void storage_save(void);
void storage_get_settings(float_settings_t *out_settings);
void storage_set_settings(const float_settings_t *new_settings);

#endif // STORAGE_H