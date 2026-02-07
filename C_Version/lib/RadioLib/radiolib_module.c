#include "radiolib_module.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief Initializes the Module structure with the provided hardware configuration.
 */
void RadioLib_Module_Create(RadioLibModule_t* mod, RadioLibHal_t* hal, uint32_t cs, uint32_t irq, uint32_t rst, uint32_t gpio) {
    mod->hal = hal;
    mod->csPin = cs;
    mod->irqPin = irq;
    mod->rstPin = rst;
    mod->gpioPin = gpio;

    // Default SPI configuration for register-access modules like SX1276
    mod->spiConfig.stream = false;
    mod->spiConfig.err = RADIOLIB_ERR_UNKNOWN;
    mod->spiConfig.cmds[0] = 0x00; // READ command
    mod->spiConfig.cmds[1] = 0x80; // WRITE command
    mod->spiConfig.cmds[2] = 0x00; // NOP
    mod->spiConfig.widths[0] = BITS_8; // Address width
    mod->spiConfig.widths[1] = BITS_0; // Command width
    mod->spiConfig.widths[2] = BITS_8; // Status width
    mod->spiConfig.statusPos = 0;
    mod->spiConfig.timeout = 1000;
}

/**
 * @brief Hardware initialization for the radio module.
 * This sets the pin modes for your specific RFM9x board connections.
 */
void RadioLib_Module_Init(RadioLibModule_t* mod) {
    mod->hal->init(mod->hal);
    
    // Initialize standard pins
    mod->hal->pinMode(mod->hal, mod->csPin, mod->hal->gpioModeOutput);
    mod->hal->digitalWrite(mod->hal, mod->csPin, mod->hal->gpioLevelHigh);
    
    if (mod->irqPin != RADIOLIB_NC) {
        mod->hal->pinMode(mod->hal, mod->irqPin, mod->hal->gpioModeInput);
    }
    
    if (mod->rstPin != RADIOLIB_NC) {
        mod->hal->pinMode(mod->hal, mod->rstPin, mod->hal->gpioModeOutput);
        mod->hal->digitalWrite(mod->hal, mod->rstPin, mod->hal->gpioLevelHigh);
    }

    // Initialize board-specific EN pin (active high for your setup)
    if (mod->enPin != RADIOLIB_NC) {
        mod->hal->pinMode(mod->hal, mod->enPin, mod->hal->gpioModeOutput);
        mod->hal->digitalWrite(mod->hal, mod->enPin, mod->hal->gpioLevelHigh);
    }

    // Initialize Radio G-pins as inputs
    for (int i = 0; i < 6; i++) {
        if (mod->radioGPins[i] != RADIOLIB_NC) {
            mod->hal->pinMode(mod->hal, mod->radioGPins[i], mod->hal->gpioModeInput);
        }
    }
}

/**
 * @brief Core SPI transfer routine.
 * Handles the low-level SPI transaction including chip-select toggling.
 */
void RadioLib_Module_SPItransfer(RadioLibModule_t* mod, uint16_t cmd, uint32_t reg, const uint8_t* dataOut, uint8_t* dataIn, size_t numBytes) {
    size_t addrLen = mod->spiConfig.widths[0] / 8;
    size_t buffLen = addrLen + numBytes;

    uint8_t* buffOut = (uint8_t*)malloc(buffLen);
    uint8_t* buffIn = (uint8_t*)malloc(buffLen);

    // Construct the address byte (reg | command bit)
    if (addrLen <= 1) {
        buffOut[0] = (uint8_t)(reg | cmd);
    } else {
        buffOut[0] = (uint8_t)((reg >> 8) | cmd);
        buffOut[1] = (uint8_t)(reg & 0xFF);
    }

    // Copy data for writing or fill with NOP for reading
    if (cmd == mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_WRITE]) {
        memcpy(&buffOut[addrLen], dataOut, numBytes);
    } else {
        memset(&buffOut[addrLen], mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_NOP], numBytes);
    }

    // Execute transaction via HAL
    mod->hal->spiBeginTransaction(mod->hal);
    mod->hal->digitalWrite(mod->hal, mod->csPin, mod->hal->gpioLevelLow);
    mod->hal->spiTransfer(mod->hal, buffOut, buffLen, buffIn);
    mod->hal->digitalWrite(mod->hal, mod->csPin, mod->hal->gpioLevelHigh);
    mod->hal->spiEndTransaction(mod->hal);

    // Capture received data if performing a read
    if (cmd == mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_READ] && dataIn != NULL) {
        memcpy(dataIn, &buffIn[addrLen], numBytes);
    }

    free(buffOut);
    free(buffIn);
}

uint8_t RadioLib_Module_SPIreadRegister(RadioLibModule_t* mod, uint32_t reg) {
    uint8_t resp = 0;
    RadioLib_Module_SPItransfer(mod, mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_READ], reg, NULL, &resp, 1);
    return resp;
}

void RadioLib_Module_SPIwriteRegister(RadioLibModule_t* mod, uint32_t reg, uint8_t data) {
    RadioLib_Module_SPItransfer(mod, mod->spiConfig.cmds[RADIOLIB_MODULE_SPI_COMMAND_WRITE], reg, &data, NULL, 1);
}

int16_t RadioLib_Module_SPIgetRegValue(RadioLibModule_t* mod, uint32_t reg, uint8_t msb, uint8_t lsb) {
    if (msb > 7 || lsb > 7 || lsb > msb) return RADIOLIB_ERR_INVALID_BIT_RANGE;
    uint8_t rawValue = RadioLib_Module_SPIreadRegister(mod, reg);
    uint8_t mask = (0xFF << lsb) & (0xFF >> (7 - msb));
    return (rawValue & mask);
}

int16_t RadioLib_Module_SPIsetRegValue(RadioLibModule_t* mod, uint32_t reg, uint8_t value, uint8_t msb, uint8_t lsb, uint8_t checkInterval, uint8_t checkMask, bool force) {
    if (msb > 7 || lsb > 7 || lsb > msb) return RADIOLIB_ERR_INVALID_BIT_RANGE;

    uint8_t currentValue = RadioLib_Module_SPIreadRegister(mod, reg);
    uint8_t mask = ~((0xFF << (msb + 1)) | (0xFF >> (8 - lsb)));

    if (((currentValue & mask) == (value & mask)) && !force) return RADIOLIB_ERR_NONE;

    uint8_t newValue = (currentValue & ~mask) | (value & mask);
    RadioLib_Module_SPIwriteRegister(mod, reg, newValue);

    return RADIOLIB_ERR_NONE;
}