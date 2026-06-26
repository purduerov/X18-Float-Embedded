#include "radiolib_sx1276.h"
#include <math.h>

void RadioLib_SX1276_Reset(RadioLibSX127x_t *chip) {
  RadioLibHal_t *hal = chip->mod->hal;
  uint32_t rst = chip->mod->rstPin;

  if (rst != RADIOLIB_NC) {
    hal->pinMode(hal, rst, hal->gpioModeOutput);

    // SX1276 reset sequence: Pull low for 100us, then release
    hal->digitalWrite(hal, rst, hal->gpioLevelHigh);
    hal->delayMicroseconds(hal, 100);
    hal->digitalWrite(hal, rst, hal->gpioLevelLow);
    hal->delay(hal, 5); // Wait for chip to stabilize
  }
}

int16_t RadioLib_SX1276_SetOutputPower(RadioLibSX127x_t *chip, int8_t power) {
  if (power < 2 || power > 17)
    return -3; // RADIOLIB_ERR_INVALID_OUTPUT_POWER

  // For RFM9x, we typically use PA_BOOST pin
  // Pout = 2 + OutputPower register value
  uint8_t paConfig = 0x80 | (power - 2); // 0x80 sets PA_SELECT_BOOST
  RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_PA_CONFIG,
                                   paConfig);

  return RADIOLIB_ERR_NONE;
}

int16_t RadioLib_SX1276_Begin(RadioLibSX127x_t *chip, float freq, float bw,
                              uint8_t sf, uint8_t cr, int8_t power,
                              uint16_t preambleLen, uint8_t syncWord) {
  // 1. Hardware Reset
  RadioLib_SX1276_Reset(chip);

  // 2. Base SX127x initialization (Checks version and enters LoRa mode)
  int16_t state = RadioLib_SX127x_Begin(chip, syncWord, preambleLen);
  if (state != RADIOLIB_ERR_NONE)
    return state;

  // 3. Apply SX1276 Specific configuration
  state = RadioLib_SX127x_SetFrequency(chip, freq);
  if (state != RADIOLIB_ERR_NONE)
    return state;

  state = RadioLib_SX127x_SetBandwidth(chip, bw);
  if (state != RADIOLIB_ERR_NONE)
    return state;

  state = RadioLib_SX127x_SetSpreadingFactor(chip, sf);
  if (state != RADIOLIB_ERR_NONE)
    return state;

  state = RadioLib_SX127x_SetCodingRate(chip, cr);
  if (state != RADIOLIB_ERR_NONE)
    return state;

  state = RadioLib_SX1276_SetOutputPower(chip, power);

  return state;
}