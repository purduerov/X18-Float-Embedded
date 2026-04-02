#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"

// Library Headers
#include "ms5837.h"
#include "bno08x_driver.h"
#include "radiolib_sx1276.h"
#include "radiolib_hal_pico.h"

// --- Hardware Configuration ---
#define I2C_PORT i2c1
#define PIN_SDA 2
#define PIN_SCL 3
#define BNO_RESET 15

#define SPI_PORT spi0
#define PIN_SCK 18
#define PIN_MOSI 19
#define PIN_MISO 20
#define PIN_CS 24
#define PIN_RST 25
#define PIN_EN 8
#define PIN_IRQ 9

#define SENSOR_TOP_OFFSET 0.465f

int tx_main() {
    stdio_init_all();
    
    // 1. HARDWARE WAKEUP
    gpio_init(PIN_EN);
    gpio_set_dir(PIN_EN, GPIO_OUT);
    gpio_put(PIN_EN, 1); 
    sleep_ms(20); 
    
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    printf("\n--- Submarine Test: Sensor Data to Radio ---\n");

    // 2. I2C INITIALIZATION
    printf("Initializing I2C Bus...");
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);
    printf("Done.\n");

    // 3. MS5837 INIT
    MS5837_t depth_sensor;
    ms5837_init_struct(&depth_sensor);
    if (!ms5837_begin(&depth_sensor, I2C_PORT)) {
        printf("MS5837 FAILED\n");
    }

    // 4. BNO085 INIT
    bno08x_driver_t imu;
    if (!bno08x_begin_i2c(&imu, I2C_PORT, BNO08x_I2CADDR_DEFAULT, BNO_RESET)) {
        printf("BNO085 FAILED\n");
    } else {
        bno08x_enable_report(&imu, SH2_ROTATION_VECTOR, 50000);
    }

    // 5. RADIOLIB SETUP (THE FIX)
    printf("Starting RadioLib Setup...\n");
    RadioLibHal_t *hal = RadioLib_Pico_Create(SPI_PORT, PIN_SCK, PIN_MOSI, PIN_MISO, 8000000);
    
    RadioLibModule_t mod;
    // CRITICAL: Zero out the struct to prevent garbage data in G-pins
    memset(&mod, 0, sizeof(RadioLibModule_t));
    
    RadioLib_Module_Create(&mod, hal, PIN_CS, PIN_IRQ, PIN_RST, RADIOLIB_NC);
    mod.enPin = PIN_EN; 
    
    // Explicitly disable unused G-pins to prevent random GPIO reconfiguration
    for (int i = 0; i < 6; i++) {
        mod.radioGPins[i] = RADIOLIB_NC;
    }

    RadioLibSX127x_t lora;
    RadioLib_SX127x_Create(&lora, &mod);

    // Added newline to ensure this prints before Begin starts
    printf("Attempting RadioLib Begin (915MHz)...\n"); 
    int16_t radio_state = RadioLib_SX1276_Begin(&lora, 915.0, 125.0, 7, 10);
    
    if (radio_state == RADIOLIB_ERR_NONE) {
        printf("Radio Success!\n");
    } else {
        printf("Radio FAILED: %d\n", radio_state);
        while(1) tight_loop_contents();
    }

    // ... Transmission Loop ...

    // --- MAIN TEST LOOP ---
    sh2_SensorValue_t sensor_value;
    char tx_buffer[128];
    float depth = 0;
    float qi = 0, qj = 0, qk = 0, qr = 0;

    printf("\nStarting Transmission Loop...\n");
    while (true) {
        // A. Update Depth
        ms5837_read(&depth_sensor);
        depth = ms5837_get_depth(&depth_sensor) - SENSOR_TOP_OFFSET;

        // B. Update IMU (Non-blocking)
        if (bno08x_get_sensor_event(&imu, &sensor_value)) {
            if (sensor_value.sensorId == SH2_ROTATION_VECTOR) {
                qi = sensor_value.un.rotationVector.i;
                qj = sensor_value.un.rotationVector.j;
                qk = sensor_value.un.rotationVector.k;
                qr = sensor_value.un.rotationVector.real;
            }
        }

        // C. Package Data (mimics Python main.py logic)
        // Format: D:Depth, Q:QuatI, QuatJ, QuatK, QuatReal
        snprintf(tx_buffer, sizeof(tx_buffer), "D:%.2f, Q:%.3f,%.3f,%.3f,%.3f", 
                 depth, qi, qj, qk, qr);

        // D. Transmit
        printf("TX: %s...", tx_buffer);
        int16_t tx_state = RadioLib_SX127x_Transmit(&lora, (uint8_t*)tx_buffer, strlen(tx_buffer));

        if (tx_state == RADIOLIB_ERR_NONE) {
            printf("OK\n");
        } else {
            printf("FAIL (%d)\n", tx_state);
        }

        sleep_ms(1000); // 1Hz update for testing
    }

    return 0;
}