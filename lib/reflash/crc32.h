#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Calculate CRC32 using the RP2040 DMA hardware sniffer.
 * 
 * @param data Pointer to the data to hash.
 * @param len Length of the data in bytes.
 * @param seed Initial seed for the calculation (default 0xFFFFFFFF).
 * @return uint32_t The resulting CRC32 checksum.
 */
uint32_t crc32_hardware(const uint8_t *data, size_t len, uint32_t seed);

/**
 * @brief A standard IEEE CRC32 implementation (for software comparison if needed).
 */
uint32_t crc32_software(const uint8_t *data, size_t len, uint32_t seed);

#endif // CRC32_H
