#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

#define SETTINGS_MAGIC 0xDEADBEEF

typedef struct {
    float kp;
    float ki;
    float kd;
    uint16_t company_number;
    uint32_t magic_number; 
} float_settings_t;

// Extern allows other files to access the active settings in RAM
extern float_settings_t current_settings;

void storage_init(void);
void storage_load(void);
void storage_save(void);

#endif // STORAGE_H