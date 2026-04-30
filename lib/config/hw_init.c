#include "hw_init.h"
#include "hw_config.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
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
    // 1. Initialize Serial
    stdio_init_all();
    
    // 2. MANDATORY WAIT for USB (Crucial for seeing first prints)
    sleep_ms(2000); 

    printf("\n\n[SYSTEM] --- X18 STARTUP DIAGNOSTIC ---\n");

#ifdef TARGET_SURFACE
    printf("[SYSTEM] Mode: SURFACE STATION\n");
    hw_wait_for_usb(SURFACE_ENABLE_USB_WAIT, 3000);
    printf("[SYSTEM] Init: Data Logger\n");
    data_logger_init();
#elif defined(TARGET_FLOAT)
    printf("[SYSTEM] Mode: FLOAT STATION\n");
    hw_wait_for_usb(FLOAT_ENABLE_USB_WAIT, 3000);
    printf("[SYSTEM] Init: Storage\n");
    storage_init();
    
    printf("[SYSTEM] Init: I2C\n");
    hw_init_i2c();

    if (depth_sensor) {
        printf("[SYSTEM] Init: MS5837 Depth Sensor\n");
        if (!hw_init_depth_sensor(depth_sensor)) {
            printf("[SYSTEM] WARNING: MS5837 NOT FOUND (Continuing without sensor)\n");
        } else {
            printf("[SYSTEM] MS5837: OK\n");
        }
    }
    printf("[SYSTEM] Init: Actuator VREF\n");
    actuator_vref_init();
#else
    printf("[SYSTEM] WARNING: No TARGET macro! Use env:float or env:surface.\n");
#endif

    if (radio_irq_callback) {
        printf("[SYSTEM] Init: LoRa Radio (SX1276)\n");
        if (!radio_setup_init(radio_irq_callback)) {
            printf("[SYSTEM] CRITICAL ERROR: Radio Hardware Failure!\n");
            return false;
        }
        printf("[SYSTEM] Radio: OK\n");
    }

    printf("[SYSTEM] --- STARTUP COMPLETE ---\n\n");
    return true;
}

void hw_init_i2c(void) {
    // 1. Nuclear Bus Clear (Bit-bang SCL if SDA is stuck LOW)
    gpio_init(PIN_SDA);
    gpio_init(PIN_SCL);
    gpio_set_dir(PIN_SDA, GPIO_IN);
    gpio_set_dir(PIN_SCL, GPIO_OUT);
    gpio_pull_up(PIN_SDA); // RP2040 internal (~50k)
    gpio_pull_up(PIN_SCL);
    sleep_ms(1);

    if (gpio_get(PIN_SDA) == 0) {
        printf("[I2C] WARNING: SDA is stuck LOW. Attempting clock-toggle recovery...\n");
        for (int i = 0; i < 16; i++) {
            gpio_put(PIN_SCL, 0);
            sleep_us(10);
            gpio_put(PIN_SCL, 1);
            sleep_us(10);
            if (gpio_get(PIN_SDA) == 1) {
                printf("[I2C] Recovery Success: Slave released SDA.\n");
                break;
            }
        }
    }

    // 2. Initialize hardware peripheral
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);
}

void hw_deinit_i2c(void) {
    i2c_deinit(I2C_PORT);
    // Reset pins to high-impedance to help clear physical hangs
    gpio_init(PIN_SDA);
    gpio_init(PIN_SCL);
    gpio_set_dir(PIN_SDA, GPIO_IN);
    gpio_set_dir(PIN_SCL, GPIO_IN);
}

bool hw_init_depth_sensor(MS5837_t *sensor) {
    ms5837_init_struct(sensor);
    sleep_ms(100); // Wait for sensor power stabilization
    if (!ms5837_begin(sensor, I2C_PORT, MS5837_02BA)) {
        return false;
    }
    return true;
}

void hw_wait_for_usb(bool enabled, uint32_t timeout_ms) {
    if (!enabled) return;
    
    uint32_t waitTime = 0;
#if PICO_STDIO_USB
    while (!stdio_usb_connected() && waitTime < timeout_ms) {
        sleep_ms(100);
        waitTime += 100;
    }
#else
    (void)waitTime;
    (void)timeout_ms;
#endif
    sleep_ms(500); 
}
