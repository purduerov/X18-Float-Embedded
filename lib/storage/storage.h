#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

// Changed magic number to force a factory reset of the flash memory
#define SETTINGS_MAGIC 0xBEEFCAF1 

typedef struct {
    float kp;
    float ki;
    float kd;
    uint16_t company_number;
    uint16_t profile_duration_s; 
    float depth_offset;
    uint16_t act_min;
    uint16_t act_max;
    uint32_t magic_number; 
} float_settings_t;

// Function prototypes for accessing and modifying settings
void storage_init(void);
void storage_save(void);
void storage_get_settings(float_settings_t *out_settings);
void storage_set_settings(const float_settings_t *new_settings);

#endif // STORAGE_H