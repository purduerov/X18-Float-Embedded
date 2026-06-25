#ifndef RADIOLIB_SX127X_H
#define RADIOLIB_SX127X_H

#include "radiolib_module.h"

// SX127x Common LoRa Registers
#define RADIOLIB_SX127X_REG_FIFO 0x00
#define RADIOLIB_SX127X_REG_OP_MODE 0x01
#define RADIOLIB_SX127X_REG_FRF_MSB 0x06
#define RADIOLIB_SX127X_REG_FRF_MID 0x07
#define RADIOLIB_SX127X_REG_FRF_LSB 0x08
#define RADIOLIB_SX127X_REG_PA_CONFIG 0x09
#define RADIOLIB_SX127X_REG_FIFO_ADDR_PTR 0x0D
#define RADIOLIB_SX127X_REG_FIFO_TX_BASE_ADDR 0x0E
#define RADIOLIB_SX127X_REG_FIFO_RX_BASE_ADDR 0x0F
#define RADIOLIB_SX127X_REG_FIFO_RX_CURRENT_ADDR 0x10
#define RADIOLIB_SX127X_REG_IRQ_FLAGS 0x12
#define RADIOLIB_SX127X_REG_RX_NB_BYTES 0x13
#define RADIOLIB_SX127X_REG_PKT_SNR_VALUE 0x19
#define RADIOLIB_SX127X_REG_PKT_RSSI_VALUE 0x1A
#define RADIOLIB_SX127X_REG_MODEM_CONFIG_1 0x1D
#define RADIOLIB_SX127X_REG_MODEM_CONFIG_2 0x1E
#define RADIOLIB_SX127X_REG_MODEM_CONFIG_3 0x26
#define RADIOLIB_SX127X_REG_PREAMBLE_MSB 0x20
#define RADIOLIB_SX127X_REG_PREAMBLE_LSB 0x21
#define RADIOLIB_SX127X_REG_PAYLOAD_LENGTH 0x22
#define RADIOLIB_SX127X_REG_DIO_MAPPING_1 0x40
#define RADIOLIB_SX127X_REG_VERSION 0x42

// Operation Modes
#define RADIOLIB_SX127X_LORA 0x80
#define RADIOLIB_SX127X_SLEEP 0x00
#define RADIOLIB_SX127X_STANDBY 0x01
#define RADIOLIB_SX127X_TX 0x03
#define RADIOLIB_SX127X_RXCONTINUOUS 0x05

// State Structure
typedef struct RadioLibSX127x
{
    RadioLibModule_t *mod;
    float frequency;
    float bandwidth;
    uint8_t spreadingFactor;
    uint8_t codingRate;
    size_t packetLength;
    bool crcEnabled;
    bool implicitHdr;

    // Internal state tracking
    uint32_t irqMap[10];
} RadioLibSX127x_t;

// Function Prototypes
void RadioLib_SX127x_Create(RadioLibSX127x_t *chip, RadioLibModule_t *mod);
int16_t RadioLib_SX127x_Begin(RadioLibSX127x_t *chip, uint8_t syncWord, uint16_t preambleLength);
int16_t RadioLib_SX127x_Transmit(RadioLibSX127x_t *chip, const uint8_t *data, size_t len);
int16_t RadioLib_SX127x_Receive(RadioLibSX127x_t *chip, uint8_t *data, size_t len);
int16_t RadioLib_SX127x_SetFrequency(RadioLibSX127x_t *chip, float freq);
int16_t RadioLib_SX127x_SetBandwidth(RadioLibSX127x_t *chip, float bw);
int16_t RadioLib_SX127x_SetSpreadingFactor(RadioLibSX127x_t *chip, uint8_t sf);
int16_t RadioLib_SX127x_SetCodingRate(RadioLibSX127x_t *chip, uint8_t cr);
int16_t RadioLib_SX127x_SetCRC(RadioLibSX127x_t *chip, bool enable);
// Starts the RX process without blocking
int16_t RadioLib_SX127x_StartReceive(RadioLibSX127x_t *chip);

// Reads the data from the FIFO after the interrupt fires
int16_t RadioLib_SX127x_ReadData(RadioLibSX127x_t *chip, uint8_t *data, size_t len);

// Helper to attach an interrupt callback to the IRQ pin
void RadioLib_SX127x_SetAction(RadioLibSX127x_t *chip, void (*cb)(void));

// Starts the TX process without blocking
int16_t RadioLib_SX127x_StartTransmit(RadioLibSX127x_t *chip, const uint8_t *data, size_t len);

// Cleans up after the TX interrupt fires
int16_t RadioLib_SX127x_FinishTransmit(RadioLibSX127x_t *chip);

float RadioLib_SX127x_GetSNR(RadioLibSX127x_t *chip);
float RadioLib_SX127x_GetRSSI(RadioLibSX127x_t *chip);
#endif