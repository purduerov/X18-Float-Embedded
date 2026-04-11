#include "hw_init.h"
#include "hw_config.h"
#include "pico/stdlib.h"
#include <stdio.h>

// Conditional Includes
#ifdef TARGET_FLOAT
#include "storage.h"
#include "actuator.h"
#endif

#ifdef TARGET_SURFACE
#include "data_logger.h"
#endif

#include "radio_setup.h"

bool system_init(void (*radio_irq_callback)(void), MS5837_t *depth_sensor) {
    stdio_init_all();
    sleep_ms(100); 
    printf("\n[SYSTEM] Booting...\n");

#ifdef TARGET_SURFACE
    printf("[SYSTEM] Target: SURFACE detected\n");
    hw_wait_for_usb(SURFACE_ENABLE_USB_WAIT, 5000);
    data_logger_init();
#elif defined(TARGET_FLOAT)
    printf("[SYSTEM] Target: FLOAT detected\n");
    hw_wait_for_usb(FLOAT_ENABLE_USB_WAIT, 5000);
    storage_init();
    
    hw_init_i2c();
    if (depth_sensor) {
        if (!hw_init_depth_sensor(depth_sensor)) {
            printf("[SYSTEM] WARNING: MS5837 NOT FOUND (Check wiring!)\n");
        } else {
            printf("[SYSTEM] MS5837 Initialized.\n");
        }
    }
    actuator_vref_init();
#else
    printf("[SYSTEM] WARNING: No TARGET macro defined!\n");
#endif

    if (radio_irq_callback) {
        if (!radio_setup_init(radio_irq_callback)) {
            printf("[SYSTEM] CRITICAL ERROR: RADIO FAILED to initialize\n");
            return false;
        }
        printf("[SYSTEM] Radio Initialized.\n");
    }

    printf("[SYSTEM] Hardware Init Complete.\n");
    return true;
}

void hw_init_i2c(void) {
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);
}

bool hw_init_depth_sensor(MS5837_t *sensor) {
    ms5837_init_struct(sensor);
    if (!ms5837_begin(sensor, I2C_PORT, MS5837_02BA)) {
        return false;
    }
    return true;
}

void hw_wait_for_usb(bool enabled, uint32_t timeout_ms) {
    if (!enabled) return;
    
    uint32_t waitTime = 0;
    while (!stdio_usb_connected() && waitTime < timeout_ms) {
        sleep_ms(100);
        waitTime += 100;
    }
}
