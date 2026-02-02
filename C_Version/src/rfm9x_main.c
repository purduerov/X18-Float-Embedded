#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "rfm9x_python.h"

// Hardware configuration
#define SPI_INST spi0
#define PIN_SCK 18
#define PIN_MOSI 19
#define PIN_MISO 20
#define PIN_CS 24  
#define PIN_RST 25 
#define LED_PIN 13 // Standard onboard LED pin for Adafruit Feather RP2350

int main()
{
    stdio_init_all();

    // LED for heartbeat
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Wait up to 5 seconds for serial monitor to connect
    // This ensures you don't miss the first print statements
    for (int i = 0; i < 10; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(250);
        gpio_put(LED_PIN, 0);
        sleep_ms(250);
    }

    printf("\n--- Starting LoRa System ---\n");

    spi_init(SPI_INST, 5 * 1000 * 1000);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    rfm9x_t rfm;
    if (!rfm9x_init(&rfm, SPI_INST, PIN_CS, PIN_RST, 915.0f))
    {
        printf("CRITICAL ERROR: RFM9x not found! Check wiring.\n");
        while (1) {
            // Blink fast to indicate error
            gpio_put(LED_PIN, 1); sleep_ms(100);
            gpio_put(LED_PIN, 0); sleep_ms(100);
        }
    }

    rfm.destination = RH_BROADCAST_ADDRESS;
    rfm.node = RH_BROADCAST_ADDRESS;
    printf("LoRa Ready. Beginning transmissions...\n");

    int count = 0;
    char msg[64];

    while (1)
    {
        // Blink LED to show activity
        gpio_put(LED_PIN, 1);
        
        snprintf(msg, sizeof(msg), "Hello from C! Count: %d", count++);
        printf("Sending [%s]... ", msg);

        if (rfm9x_send(&rfm, (uint8_t *)msg, strlen(msg), 13)) {
            printf("Success!\n");
        } else {
            printf("Failed!\n");
        }

        gpio_put(LED_PIN, 0);
        sleep_ms(2000);
    }

    return 0;
}