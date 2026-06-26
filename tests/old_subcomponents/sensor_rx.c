#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Library Headers
#include "radiolib_sx1276.h"
#include "radiolib_hal_pico.h"

// --- Hardware Configuration (Matches your TX setup) ---
#define SPI_PORT spi0
#define PIN_SCK 18
#define PIN_MOSI 19
#define PIN_MISO 20
#define PIN_CS 24
#define PIN_RST 25
#define PIN_EN 8
#define PIN_IRQ 9

void parse_submarine_data(char* data) {
    float depth = 0;
    float qi = 0, qj = 0, qk = 0, qr = 0;

    // Search for the tags we defined in the TX script
    // Format: "D:%.2f, Q:%.3f,%.3f,%.3f,%.3f"
    char* depth_ptr = strstr(data, "D:");
    char* quat_ptr = strstr(data, "Q:");

    if (depth_ptr) {
        sscanf(depth_ptr, "D:%f", &depth);
    }
    if (quat_ptr) {
        sscanf(quat_ptr, "Q:%f,%f,%f,%f", &qi, &qj, &qk, &qr);
    }

    printf("\n>>> RECEIVED PACKET <<<\n");
    printf("DEPTH: %.2f m\n", depth);
    printf("IMU:   I:%.3f J:%.3f K:%.3f Real:%.3f\n", qi, qj, qk, qr);
    printf("------------------------\n");
}

int rx_main() {
    stdio_init_all();

    // 1. HARDWARE WAKEUP (EN PIN)
    gpio_init(PIN_EN);
    gpio_set_dir(PIN_EN, GPIO_OUT);
    gpio_put(PIN_EN, 1); 
    sleep_ms(20);

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    printf("--- Submarine Ground Station: Receiver Starting ---\n");

    // 2. RADIOLIB SETUP
    RadioLibHal_t *hal = RadioLib_Pico_Create(SPI_PORT, PIN_SCK, PIN_MOSI, PIN_MISO, 8000000);
    
    RadioLibModule_t mod;
    memset(&mod, 0, sizeof(RadioLibModule_t)); // Prevention of garbage data
    RadioLib_Module_Create(&mod, hal, PIN_CS, PIN_IRQ, PIN_RST, RADIOLIB_NC);
    mod.enPin = PIN_EN;

    // Disable unused G-pins
    for (int i = 0; i < 6; i++) mod.radioGPins[i] = RADIOLIB_NC;

    RadioLibSX127x_t lora;
    RadioLib_SX127x_Create(&lora, &mod);

    // 3. INITIALIZE RADIO (Must match TX settings: 915MHz, 125kHz BW, SF7)
    printf("Initializing SX1276...");
    int16_t state = RadioLib_SX1276_Begin(&lora, 915.0, 125.0, 7, 5, 10, 8, 0x12);

    if (state == RADIOLIB_ERR_NONE) {
        printf("Success!\nWaiting for incoming packets...\n");
    } else {
        printf("FAILED (%d)\n", state);
        while (1) tight_loop_contents();
    }

    uint8_t rx_buffer[256];

    while (true) {
        // This function blocks until a packet is received or an error occurs
        int16_t len = RadioLib_SX127x_Receive(&lora, rx_buffer, sizeof(rx_buffer) - 1);

        if (len > 0) {
            rx_buffer[len] = '\0'; // Null-terminate the string
            parse_submarine_data((char*)rx_buffer);
        } 
        else if (len == RADIOLIB_ERR_CRC_MISMATCH) {
            printf("ERROR: CRC Mismatch (Corrupted Data)\n");
        } 
        else if (len != -1) { // -1 is usually a timeout/no-data-yet
            printf("ERROR: Receive failed with code %d\n", len);
        }
    }

    return 0;
}