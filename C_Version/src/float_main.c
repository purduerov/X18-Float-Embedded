#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ms5837.h"
#include <stdio.h>
#include <string.h>

// --- Custom Library Includes ---
#include "packets.h"
#include "storage.h"
#include "radio_setup.h"
#include "float_fsm.h"

// --- I2C / Sensor Setup ---
#define I2C_PORT i2c1
#define PIN_SDA 2
#define PIN_SCL 3

static float_fsm_t global_fsm;

void onInterrupt(void) {
    float_fsm_on_interrupt(&global_fsm);
}

int main()
{
    stdio_init_all();

    uint32_t waitTime = 0;
    while (!stdio_usb_connected() && waitTime < 5000)
    {
        sleep_ms(100);
        waitTime += 100;
    }

    printf("\n\n=== MATE Float Station Booting (Refactored) ===\n");

    // --- Initialize Persistent Storage ---
    storage_init(); 

    // --- Initialize I2C and MS5837 ---
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);

    MS5837_t depth_sensor;
    ms5837_init_struct(&depth_sensor);
    if (!ms5837_begin(&depth_sensor, I2C_PORT, MS5837_02BA))
    {
        printf("CRITICAL ERROR: MS5837 FAILED to initialize\n");
    }

    // --- Initialize Radio ---
    if (!radio_setup_init(onInterrupt)) {
        printf("Radio init failed! Halting.\n");
        while (true) sleep_ms(1000);
    }

    // --- Initialize State Machine ---
    float_fsm_init(&global_fsm, &depth_sensor);

    printf("Float System Ready.\n");

    while (true)
    {
        float_fsm_update(&global_fsm);
        sleep_ms(1);
    }

    return 0;
}