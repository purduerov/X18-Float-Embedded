#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

// Changed magic number to force a factory reset of the flash memory
#define SETTINGS_MAGIC 0xBEEFCAFE 

typedef struct {
    float kp;
    float ki;
    float kd;
    uint16_t company_number;
    uint16_t profile_duration_s; 
    uint32_t magic_number; 
} float_settings_t;

extern float_settings_t current_settings;

void storage_init(void);
void storage_load(void);
void storage_save(void);

#endif // STORAGE_H