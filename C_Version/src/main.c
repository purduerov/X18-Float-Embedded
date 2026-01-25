#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Hardware Configuration
// For Feather RP2040 standard SDA/SCL pins:
#define I2C_PORT i2c1
#define SDA_PIN 2
#define SCL_PIN 3

int main() {
    // Initialize standard I/O for serial monitor output
    stdio_init_all();

    // Give the serial terminal time to connect
    sleep_ms(8000);
    printf("\n--- I2C Bus Scanner ---\n");

    // Initialize I2C at a conservative 100kHz for scanning
    i2c_init(I2C_PORT, 10 * 1000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    // Enable internal pull-ups (required if external ones are missing)
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    printf("Scanning bus i2c1 (SDA: GPIO %d, SCL: GPIO %d)...\n", SDA_PIN, SCL_PIN);
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        // Attempt to read 1 byte from the current address.
        // If a device is present, it will acknowledge the address.
        uint8_t rxdata;
        int result = i2c_read_blocking(I2C_PORT, addr, &rxdata, 1, false);

        if (result >= 0) {
            // Device found
            printf("%02x ", addr);
        } else {
            // No device at this address
            printf("-- ");
        }

        if (addr % 16 == 15) {
            printf("\n");
        }
    }

    printf("\nScan Complete.\n");

    // Infinite loop to keep the program alive
    while (true) {
        tight_loop_contents();
    }

    return 0;
}