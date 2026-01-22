#include "ms5837.h"
#include <math.h>

// Internal command constants
#define MS5837_RESET_CMD 0x1E
#define MS5837_ADC_READ 0x00
#define MS5837_PROM_READ 0xA0
#define MS5837_CONVERT_D1_8192 0x4A
#define MS5837_CONVERT_D2_8192 0x5A

// Model detection thresholds
#define MS5837_02BA_MAX_SENSITIVITY 49000
#define MS5837_02BA_30BA_SEPARATION 37000
#define MS5837_30BA_MIN_SENSITIVITY 26000


// Internal helper for CRC4 verification
static uint8_t ms5837_crc4(uint16_t n_prom[]) {
    uint16_t n_rem = 0;
    n_prom[0] = ((n_prom[0]) & 0x0FFF);
    n_prom[7] = 0;

    for (uint8_t i = 0; i < 16; i++) {
        if (i % 2 == 1) {
            n_rem ^= (uint16_t)((n_prom[i >> 1]) & 0x00FF);
        } else {
            n_rem ^= (uint16_t)(n_prom[i >> 1] >> 8);
        }
        for (uint8_t n_bit = 8; n_bit > 0; n_bit--) {
            if (n_rem & 0x8000) {
                n_rem = (n_rem << 1) ^ 0x3000;
            } else {
                n_rem = (n_rem << 1);
            }
        }
    }
    return ((n_rem >> 12) & 0x000F);
}