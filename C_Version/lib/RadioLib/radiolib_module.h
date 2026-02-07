#ifndef RADIOLIB_MODULE_H
#define RADIOLIB_MODULE_H

#include "radiolib_hal_pico.h"
#include <stdbool.h>

// Status and Error Codes
#define RADIOLIB_ERR_NONE             (0)
#define RADIOLIB_ERR_UNKNOWN          (-1)
#define RADIOLIB_ERR_SPI_WRITE_FAILED (-7)
#define RADIOLIB_ERR_INVALID_BIT_RANGE (-5)
#define RADIOLIB_ERR_CRC_MISMATCH     (-6)  // <--- Added this line
#define RADIOLIB_NC                   (0xFFFFFFFF)

// SPI Command Positions
#define RADIOLIB_MODULE_SPI_COMMAND_READ    (0)
#define RADIOLIB_MODULE_SPI_COMMAND_WRITE   (1)
#define RADIOLIB_MODULE_SPI_COMMAND_NOP     (2)
#define RADIOLIB_MODULE_SPI_COMMAND_STATUS  (3)

// RF Switch Constants
#define RFSWITCH_MAX_PINS   (5)
#define RFSWITCH_PIN_FLAG   (0x01UL << 31)

typedef enum {
    BITS_0 = 0,
    BITS_8 = 8,
    BITS_16 = 16,
    BITS_24 = 24,
    BITS_32 = 32,
} BitWidth_t;

// SPI configuration structure
typedef struct {
    bool stream;
    int16_t err;
    uint16_t cmds[4];
    BitWidth_t widths[3];
    uint8_t statusPos;
    int16_t (*parseStatusCb)(uint8_t in);
    RadioLibTime_t timeout;
} SPIConfig_t;

// RF switch mode structure
typedef struct {
    uint8_t mode;
    uint32_t values[RFSWITCH_MAX_PINS];
} RfSwitchMode_t;

// The Module structure
typedef struct RadioLibModule {
    RadioLibHal_t* hal;
    uint32_t csPin;
    uint32_t irqPin;
    uint32_t rstPin;
    uint32_t gpioPin;
    
    // Physical hardware specific to your setup
    uint32_t enPin;
    uint32_t radioGPins[6];

    SPIConfig_t spiConfig;
    
    uint32_t rfSwitchPins[RFSWITCH_MAX_PINS];
    const RfSwitchMode_t* rfSwitchTable;
} RadioLibModule_t;

// Function Prototypes
void RadioLib_Module_Create(RadioLibModule_t* mod, RadioLibHal_t* hal, uint32_t cs, uint32_t irq, uint32_t rst, uint32_t gpio);
void RadioLib_Module_Init(RadioLibModule_t* mod);
int16_t RadioLib_Module_SPIgetRegValue(RadioLibModule_t* mod, uint32_t reg, uint8_t msb, uint8_t lsb);
int16_t RadioLib_Module_SPIsetRegValue(RadioLibModule_t* mod, uint32_t reg, uint8_t value, uint8_t msb, uint8_t lsb, uint8_t checkInterval, uint8_t checkMask, bool force);
uint8_t RadioLib_Module_SPIreadRegister(RadioLibModule_t* mod, uint32_t reg);
void RadioLib_Module_SPIwriteRegister(RadioLibModule_t* mod, uint32_t reg, uint8_t data);
void RadioLib_Module_SPItransfer(RadioLibModule_t* mod, uint16_t cmd, uint32_t reg, const uint8_t* dataOut, uint8_t* dataIn, size_t numBytes);

#endif