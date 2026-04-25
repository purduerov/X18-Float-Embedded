#include "radiolib_sx127x.h"
#include <math.h>
#include <stdio.h> // Required for debug prints

int16_t RadioLib_SX127x_SetFrequency(RadioLibSX127x_t* chip, float freq) {
    // Check frequency limits (SX1276 range: 137 - 1020 MHz)
    if (freq < 137.0 || freq > 1020.0) return -3; // RADIOLIB_ERR_INVALID_FREQUENCY

    // Calculate carrier frequency steps (f_step = f_osc / 2^19 = 32MHz / 524288)
    uint32_t frf = (uint32_t)((freq * 1000000.0) / 61.03515625);

    // Write frequency bytes to registers
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_FRF_MSB, (uint8_t)((frf >> 16) & 0xFF));
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_FRF_MID, (uint8_t)((frf >> 8) & 0xFF));
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_FRF_LSB, (uint8_t)(frf & 0xFF));

    chip->frequency = freq;
    return RADIOLIB_ERR_NONE;
}

int16_t RadioLib_SX127x_SetBandwidth(RadioLibSX127x_t* chip, float bw) {
    uint8_t bwVal = 0xFF;
    // Map float BW to register value (0-9)
    if (bw == 7.8f) bwVal = 0;
    else if (bw == 10.4f) bwVal = 1;
    else if (bw == 15.6f) bwVal = 2;
    else if (bw == 20.8f) bwVal = 3;
    else if (bw == 31.25f) bwVal = 4;
    else if (bw == 41.7f) bwVal = 5;
    else if (bw == 62.5f) bwVal = 6;
    else if (bw == 125.0f) bwVal = 7;
    else if (bw == 250.0f) bwVal = 8;
    else if (bw == 500.0f) bwVal = 9;

    if (bwVal == 0xFF) return -3; // RADIOLIB_ERR_INVALID_BANDWIDTH

    // Set bandwidth in ModemConfig1 (bits 7-4)
    int16_t state = RadioLib_Module_SPIsetRegValue(chip->mod, RADIOLIB_SX127X_REG_MODEM_CONFIG_1, bwVal << 4, 7, 4, 5, 0xFF, false);
    if (state == RADIOLIB_ERR_NONE) chip->bandwidth = bw;
    return state;
}

int16_t RadioLib_SX127x_SetSpreadingFactor(RadioLibSX127x_t* chip, uint8_t sf) {
    if (sf < 6 || sf > 12) return -3; // RADIOLIB_ERR_INVALID_SPREADING_FACTOR

    // Set spreading factor in ModemConfig2 (bits 7-4)
    int16_t state = RadioLib_Module_SPIsetRegValue(chip->mod, RADIOLIB_SX127X_REG_MODEM_CONFIG_2, sf << 4, 7, 4, 5, 0xFF, false);
    if (state != RADIOLIB_ERR_NONE) return state;

    // Handle LowDataRateOptimize (required for symbol duration > 16.38ms)
    // Symbol duration = 2^SF / BW. At BW=125kHz:
    // SF11 = 2048 / 125000 = 16.384ms
    // SF12 = 4096 / 125000 = 32.768ms
    if (sf >= 11) {
        state = RadioLib_Module_SPIsetRegValue(chip->mod, RADIOLIB_SX127X_REG_MODEM_CONFIG_3, 0x08, 3, 3, 5, 0xFF, false);
    } else {
        state = RadioLib_Module_SPIsetRegValue(chip->mod, RADIOLIB_SX127X_REG_MODEM_CONFIG_3, 0x00, 3, 3, 5, 0xFF, false);
    }

    if (state == RADIOLIB_ERR_NONE) chip->spreadingFactor = sf;
    return state;
}

int16_t RadioLib_SX127x_SetCodingRate(RadioLibSX127x_t* chip, uint8_t cr) {
    if (cr < 5 || cr > 8) return -3; // RADIOLIB_ERR_INVALID_CODING_RATE

    // Coding rate is mapped to bits 3-1 of ModemConfig1 as (cr - 4)
    uint8_t crVal = cr - 4;
    int16_t state = RadioLib_Module_SPIsetRegValue(chip->mod, RADIOLIB_SX127X_REG_MODEM_CONFIG_1, crVal << 1, 3, 1, 5, 0xFF, false);
    if (state == RADIOLIB_ERR_NONE) chip->codingRate = cr;
    return state;
}

int16_t RadioLib_SX127x_SetCRC(RadioLibSX127x_t* chip, bool enable) {
    // CRC is bit 2 of ModemConfig2
    uint8_t val = enable ? 0x04 : 0x00;
    int16_t state = RadioLib_Module_SPIsetRegValue(chip->mod, RADIOLIB_SX127X_REG_MODEM_CONFIG_2, val, 2, 2, 5, 0xFF, false);
    if (state == RADIOLIB_ERR_NONE) chip->crcEnabled = enable;
    return state;
}

void RadioLib_SX127x_Create(RadioLibSX127x_t* chip, RadioLibModule_t* mod) {
    chip->mod = mod;
    chip->frequency = 434.0;
    chip->bandwidth = 125.0;
    chip->spreadingFactor = 7;
    chip->codingRate = 5;
    chip->packetLength = 0;
    chip->crcEnabled = true;
    chip->implicitHdr = false;
}

static int16_t setMode(RadioLibSX127x_t* chip, uint8_t mode) {
    return RadioLib_Module_SPIsetRegValue(chip->mod, RADIOLIB_SX127X_REG_OP_MODE, mode, 2, 0, 5, 0xFF, false);
}

int16_t RadioLib_SX127x_Begin(RadioLibSX127x_t* chip, uint8_t syncWord, uint16_t preambleLength) {
    // 1. Reset and Init Module
    RadioLib_Module_Init(chip->mod);
    
    // 2. Check SPI connection
    uint8_t version = RadioLib_Module_SPIreadRegister(chip->mod, RADIOLIB_SX127X_REG_VERSION);
    if (version == 0x00 || version == 0xFF) return -2; // RADIOLIB_ERR_CHIP_NOT_FOUND

    // 3. FORCE SLEEP MODE (Required to switch to LoRa)
    // Write 0x00 (Sleep + FSK) to OpMode
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_OP_MODE, RADIOLIB_SX127X_SLEEP);
    
    // 4. Enable LoRa Mode (Must be done in Sleep)
    // Write 0x80 (Sleep + LoRa)
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_OP_MODE, RADIOLIB_SX127X_LORA | RADIOLIB_SX127X_SLEEP);
    
    // 5. Enter LoRa Standby
    // Write 0x81 (Standby + LoRa)
    int16_t state = RadioLib_Module_SPIsetRegValue(chip->mod, RADIOLIB_SX127X_REG_OP_MODE, RADIOLIB_SX127X_LORA | RADIOLIB_SX127X_STANDBY, 7, 0, 5, 0xFF, false);
    if (state != RADIOLIB_ERR_NONE) return state;

    // 6. Configure Sync Word and Preamble
    RadioLib_Module_SPIwriteRegister(chip->mod, 0x39, syncWord); // SX127X_REG_SYNC_WORD
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_PREAMBLE_MSB, (uint8_t)((preambleLength >> 8) & 0xFF));
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_PREAMBLE_LSB, (uint8_t)(preambleLength & 0xFF));

    // 7. Initialize default hardware state matching software structure
    RadioLib_SX127x_SetCodingRate(chip, chip->codingRate);
    RadioLib_SX127x_SetCRC(chip, chip->crcEnabled);

    return RADIOLIB_ERR_NONE;
}

int16_t RadioLib_SX127x_Transmit(RadioLibSX127x_t* chip, const uint8_t* data, size_t len) {
    // 1. Standby mode
    setMode(chip, RADIOLIB_SX127X_STANDBY);

    // 2. Clear all IRQ flags (CRITICAL FIX: Clears the 0x15 ghost flags)
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS, 0xFF);

    // 3. Configure DIO0 to TxDone (0x40 maps DIO0 -> 00)
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_DIO_MAPPING_1, 0x40);

    // 4. Set FIFO pointers
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO_ADDR_PTR, 0);
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO_TX_BASE_ADDR, 0);
    
    // 5. Write data to FIFO
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_PAYLOAD_LENGTH, (uint8_t)len);
    for (size_t i = 0; i < len; i++) {
        RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO, data[i]);
    }

    // 6. Start TX
    setMode(chip, RADIOLIB_SX127X_TX);

    // 7. Wait for TX Done (polling IRQ pin G0/9)
    RadioLibHal_t* hal = chip->mod->hal;
    RadioLibTime_t start = hal->millis(hal);
    while (!hal->digitalRead(hal, chip->mod->irqPin)) {
        
        // --- TIMEOUT DEBUG BLOCK ---
        if (hal->millis(hal) - start > 1000) {
            uint8_t flags = RadioLib_Module_SPIreadRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS);
            uint8_t mode = RadioLib_Module_SPIreadRegister(chip->mod, RADIOLIB_SX127X_REG_OP_MODE);
            
            printf("\n--- TIMEOUT DIAGNOSTICS ---\n");
            printf("IRQ Flags: 0x%02X (Expected: 0x08 for TxDone)\n", flags);
            printf("Op Mode:   0x%02X (Expected: 0x83 for LoRa TX)\n", mode);
            
            if (flags & 0x08) {
                printf("CONCLUSION: Radio finished! Pico Pin %lu is not reading HIGH.\n", chip->mod->irqPin);
                printf("CHECK: Wire connection from DIO0 to Pin 12 (GPIO9).\n");
            } else if (mode != 0x83) {
                 printf("CONCLUSION: Radio aborted TX (Mode switched to 0x%02X).\n", mode);
                 printf("CHECK: Power supply (Brownout?) or Over-Current Protection.\n");
            } else {
                printf("CONCLUSION: Radio is in TX mode but stuck.\n");
            }
            
            // Clear flags and exit
            RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS, 0xFF);
            return -5; // RADIOLIB_ERR_TX_TIMEOUT
        }
        // ---------------------------

        hal->yield(hal);
    }

    // 8. Clear IRQ flags and return to standby
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS, 0xFF);
    return setMode(chip, RADIOLIB_SX127X_STANDBY);
}

int16_t RadioLib_SX127x_Receive(RadioLibSX127x_t* chip, uint8_t* data, size_t len) {
    // 1. Standby mode
    setMode(chip, RADIOLIB_SX127X_STANDBY);

    // 2. Map DIO0 to RxDone (00)
    // (Crucial: Transmit set this to 0x40; we MUST set it back to 0x00 for RX)
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_DIO_MAPPING_1, 0x00);

    // 3. Clear IRQ flags
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS, 0xFF);

    // 4. Enter RX Continuous
    setMode(chip, RADIOLIB_SX127X_RXCONTINUOUS);

    // 5. Wait for RX Done
    RadioLibHal_t* hal = chip->mod->hal;
    while (!hal->digitalRead(hal, chip->mod->irqPin)) {
        hal->yield(hal);
    }

    // 6. Check for CRC Error (IRQ Flag Bit 5)
    uint8_t flags = RadioLib_Module_SPIreadRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS);
    if (flags & 0x20) { // PayloadCrcError
        RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS, 0xFF);
        return RADIOLIB_ERR_CRC_MISMATCH; // Return -7
    }

    // 7. Read packet length
    uint8_t length = RadioLib_Module_SPIreadRegister(chip->mod, RADIOLIB_SX127X_REG_RX_NB_BYTES);
    
    // 8. Set FIFO pointer to current packet address
    uint8_t currentAddr = RadioLib_Module_SPIreadRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO_RX_CURRENT_ADDR);
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO_ADDR_PTR, currentAddr);

    // 9. Read data
    // Ensure we don't overflow the buffer provided
    size_t readLen = (len < length) ? len : length; 
    for (size_t i = 0; i < readLen; i++) {
        data[i] = RadioLib_Module_SPIreadRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO);
    }

    // 10. Clear Flags and go back to Standby
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS, 0xFF);
    setMode(chip, RADIOLIB_SX127X_STANDBY);
    
    return (int16_t)length; // Return number of bytes received
}

int16_t RadioLib_SX127x_StartReceive(RadioLibSX127x_t* chip) {
    // 1. Standby mode
    setMode(chip, RADIOLIB_SX127X_STANDBY);

    // 2. Map DIO0 to RxDone (0x00 maps DIO0 to RxDone)
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_DIO_MAPPING_1, 0x00);

    // 3. Clear IRQ flags
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS, 0xFF);

    // 4. Enter RX Continuous (or RX_SINGLE if you prefer)
    return setMode(chip, RADIOLIB_SX127X_RXCONTINUOUS);
}

int16_t RadioLib_SX127x_ReadData(RadioLibSX127x_t* chip, uint8_t* data, size_t len) {
    // 1. Check for CRC Error (IRQ Flag Bit 5)
    uint8_t flags = RadioLib_Module_SPIreadRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS);
    if (flags & 0x20) { 
        RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS, 0xFF);
        return RADIOLIB_ERR_CRC_MISMATCH; 
    }

    // 2. Read packet length
    uint8_t length = RadioLib_Module_SPIreadRegister(chip->mod, RADIOLIB_SX127X_REG_RX_NB_BYTES);
    
    // 3. Set FIFO pointer to current packet address
    uint8_t currentAddr = RadioLib_Module_SPIreadRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO_RX_CURRENT_ADDR);
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO_ADDR_PTR, currentAddr);

    // 4. Read data safely
    size_t readLen = (len < length) ? len : length; 
    for (size_t i = 0; i < readLen; i++) {
        data[i] = RadioLib_Module_SPIreadRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO);
    }

    // 5. Clear Flags 
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS, 0xFF);
    
    // Note: If using RXCONTINUOUS, the radio stays in RX. 
    // If you want to stop receiving, uncomment the next line:
    // setMode(chip, RADIOLIB_SX127X_STANDBY);
    
    return (int16_t)length;
}

void RadioLib_SX127x_SetAction(RadioLibSX127x_t* chip, void (*cb)(void)) {
    RadioLibHal_t* hal = chip->mod->hal;
    if (chip->mod->irqPin != RADIOLIB_NC) {
        hal->attachInterrupt(hal, chip->mod->irqPin, cb, hal->gpioInterruptRising);
    }
}

int16_t RadioLib_SX127x_StartTransmit(RadioLibSX127x_t* chip, const uint8_t* data, size_t len) {
    // 1. Standby mode
    setMode(chip, RADIOLIB_SX127X_STANDBY);

    // 2. Clear all IRQ flags
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS, 0xFF);

    // 3. Configure DIO0 to TxDone (0x40 maps DIO0 to TxDone)
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_DIO_MAPPING_1, 0x40);

    // 4. Set FIFO pointers
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO_ADDR_PTR, 0);
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO_TX_BASE_ADDR, 0);
    
    // 5. Write data to FIFO
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_PAYLOAD_LENGTH, (uint8_t)len);
    for (size_t i = 0; i < len; i++) {
        RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_FIFO, data[i]);
    }

    // 6. Start TX and return immediately
    return setMode(chip, RADIOLIB_SX127X_TX);
}

int16_t RadioLib_SX127x_FinishTransmit(RadioLibSX127x_t* chip) {
    // Clear IRQ flags and return to standby
    RadioLib_Module_SPIwriteRegister(chip->mod, RADIOLIB_SX127X_REG_IRQ_FLAGS, 0xFF);
    return setMode(chip, RADIOLIB_SX127X_STANDBY);
}