#ifndef RADIOLIB_SX1276_H
#define RADIOLIB_SX1276_H

#include "radiolib_sx127x.h"

// Chip Version for SX1276/RFM95/96
#define RADIOLIB_SX127X_CHIP_VERSION_SX1276     0x12

// Frequency Ranges
#define RADIOLIB_SX1276_FREQ_MIN                137.0
#define RADIOLIB_SX1276_FREQ_MAX                1020.0

// Function Prototypes
/**
 * @brief Initialize the SX1276/RFM9x module with standard LoRa settings.
 * @param chip Pointer to the SX127x state structure.
 * @param freq Carrier frequency in MHz (e.g., 915.0 or 434.0).
 * @param bw Bandwidth in kHz (default 125.0).
 * @param sf Spreading factor (6 - 12).
 * @param cr Coding rate (5 - 8).
 * @param power Output power in dBm (2 - 17).
 * @param preambleLen Preamble length in symbols (default 8).
 * @param syncWord LoRa sync word for network filtering.
 */
int16_t RadioLib_SX1276_Begin(RadioLibSX127x_t* chip, float freq, float bw, uint8_t sf, uint8_t cr, int8_t power, uint16_t preambleLen, uint8_t syncWord);

/**
 * @brief Performs a hard reset of the radio using the RST pin.
 */
void RadioLib_SX1276_Reset(RadioLibSX127x_t* chip);

/**
 * @brief Configure the transmission output power.
 */
int16_t RadioLib_SX1276_SetOutputPower(RadioLibSX127x_t* chip, int8_t power);

#endif