#include "hw_init.h"
#include "hw_config.h"
#include "pico/stdlib.h"
#include <stdio.h>

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
