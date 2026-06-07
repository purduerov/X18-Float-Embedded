#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

uint32_t crc32_software(const uint8_t *data, size_t len, uint32_t seed) {
    uint32_t crc = seed;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

int main() {
    uint8_t data[88000];
    for (int i = 0; i < 88000; i++) data[i] = 0xFF;
    uint32_t crc = crc32_software(data, 88000, 0xFFFFFFFF);
    crc ^= 0xFFFFFFFF;
    printf("CRC: 0x%08X\n", crc);
    return 0;
}
