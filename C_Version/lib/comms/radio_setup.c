#include "radio_setup.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// --- Hardcoded Radio Pins ---
const uint32_t SPI_MOSI = 19;
const uint32_t SPI_MISO = 20;
const uint32_t SPI_SCK = 18;
const uint32_t CS_PIN = 24;
const uint32_t RST_PIN = 25;
const uint32_t EN_PIN = 8;
const uint32_t IRQ_PIN = 9;

RadioLibHal_t *hal;
RadioLibModule_t radioModule;
RadioLibSX127x_t lora;

bool radio_setup_init(void (*interrupt_callback)(void)) {
    // Power on the radio module
    gpio_init(EN_PIN);
    gpio_set_dir(EN_PIN, GPIO_OUT);
    gpio_put(EN_PIN, 1);
    sleep_ms(100);

    // Initialize hardware abstraction
    hal = RadioLib_Pico_Create(spi0, SPI_SCK, SPI_MOSI, SPI_MISO, 8000000);
    memset(&radioModule, 0, sizeof(RadioLibModule_t)); 
    RadioLib_Module_Create(&radioModule, hal, CS_PIN, IRQ_PIN, RST_PIN, RADIOLIB_NC);
    radioModule.enPin = EN_PIN;

    uint32_t gPins[] = {RADIOLIB_NC, 29, 6, 7, 10, 11};
    for (int i = 0; i < 6; i++) {
        radioModule.radioGPins[i] = gPins[i];
    }

    RadioLib_SX127x_Create(&lora, &radioModule);

    printf("[RADIO] Initializing SX1276...\n");
    int16_t status = RadioLib_SX1276_Begin(&lora, 915.0, 125.0, 7, 10);
    
    if (status != RADIOLIB_ERR_NONE) {
        printf("[RADIO] CRITICAL ERROR: Init failed, code %d\n", status);
        return false;
    }

    // Attach the interrupt routine provided by the main file
    RadioLib_SX127x_SetAction(&lora, interrupt_callback);
    printf("[RADIO] Init Success!\n");
    
    return true;
}