#include "pico/stdlib.h"
#include "radiolib_hal_pico.h"
#include "radiolib_sx1276.h"
#include <stdio.h>
#include <string.h>

const uint32_t SPI_MOSI = 19;
const uint32_t SPI_MISO = 20;
const uint32_t SPI_SCK = 18;
const uint32_t CS_PIN = 24;
const uint32_t RST_PIN = 25;
const uint32_t EN_PIN = 8;
const uint32_t IRQ_PIN = 9; // Make sure this is wired to Physical Pin 12

int radio_rx_main() {
    stdio_init_all();
    
    // Power up sequence
    gpio_init(EN_PIN);
    gpio_set_dir(EN_PIN, GPIO_OUT);
    gpio_put(EN_PIN, 1);
    sleep_ms(100);

    // Wait for USB console
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    printf("Starting RadioLib RX Demo...\n");

    // Initialize HAL
    RadioLibHal_t *hal = RadioLib_Pico_Create(spi0, SPI_SCK, SPI_MOSI, SPI_MISO, 8000000);
    RadioLibModule_t radioModule;
    RadioLib_Module_Create(&radioModule, hal, CS_PIN, IRQ_PIN, RST_PIN, RADIOLIB_NC);
    radioModule.enPin = EN_PIN;
    
    // Init G-Pins (Optional, but good practice to prevent floating)
    uint32_t gPins[] = {RADIOLIB_NC, 29, 6, 7, 10, 11};
    for (int i = 0; i < 6; i++) radioModule.radioGPins[i] = gPins[i];

    RadioLibSX127x_t lora;
    RadioLib_SX127x_Create(&lora, &radioModule);

    printf("Initializing SX1276...");
    // Use the same settings as your Transmitter! (Freq, BW, SF, CR)
    int16_t state = RadioLib_SX1276_Begin(&lora, 915.0, 125.0, 7, 10);

    if (state == RADIOLIB_ERR_NONE) {
        printf("success!\n");
    } else {
        printf("failed, code %d\n", state);
        while (true) sleep_ms(100);
    }

    uint8_t buffer[256];
    
    while (true) {
        printf("Waiting for packet...\n");
        
        // This function blocks until a packet is received
        int16_t len = RadioLib_SX127x_Receive(&lora, buffer, sizeof(buffer) - 1);

        if (len > 0) {
            // Null terminate for printing
            buffer[len] = 0;
            printf("Received packet! (Len: %d)\n", len);
            printf("Data: %s\n", (char*)buffer);
            
            // Read RSSI (0x1A) and SNR (0x19) manually for now
            int8_t rssi = RadioLib_Module_SPIreadRegister(lora.mod, RADIOLIB_SX127X_REG_PKT_RSSI_VALUE) - 157;
            int8_t snr = (int8_t)RadioLib_Module_SPIreadRegister(lora.mod, RADIOLIB_SX127X_REG_PKT_SNR_VALUE) / 4;
            printf("RSSI: %d dBm, SNR: %d dB\n", rssi, snr);
            
        } else if (len == RADIOLIB_ERR_CRC_MISMATCH) {
            printf("Error: CRC Mismatch (Corrupted packet)\n");
        } else {
            printf("Error: Code %d\n", len);
        }
    }

    return 0;
}