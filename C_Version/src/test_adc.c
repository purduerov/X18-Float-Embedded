#include "pico/stdlib.h"

#define PIN_EXT 12  
#define PIN_RET 13

int adc_main() {
    // Initialize the SDK standard I/O
    stdio_init_all();

    // Initialize GPIO pins
    gpio_init(PIN_EXT);
    gpio_init(PIN_RET);
    gpio_init(27);
    // Set both pins as outputs
    gpio_set_dir(PIN_EXT, GPIO_OUT);
    gpio_set_dir(PIN_RET, GPIO_OUT);
    gpio_set_dir(27, GPIO_OUT);

    gpio_put(27, 1); // Power on the system

    while (true) {
        // Direction 1: Extend
        gpio_put(PIN_EXT, 1);
        gpio_put(PIN_RET, 0);
        sleep_ms(500);

        // Direction 2: Retract
        gpio_put(PIN_EXT, 0);
        gpio_put(PIN_RET, 1);
        sleep_ms(500);
        
        // Optional: Stop briefly to prevent high current spikes
        gpio_put(PIN_EXT, 0);
        gpio_put(PIN_RET, 0);
        sleep_ms(500);
    }

    return 0;
}