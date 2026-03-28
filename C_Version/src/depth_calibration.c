#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"

// Library Headers
#include "ms5837.h"
#include "radiolib_sx1276.h"
#include "radiolib_hal_pico.h"

// --- Hardware Configuration ---
#define I2C_PORT i2c1
#define PIN_SDA 2
#define PIN_SCL 3

#define SPI_PORT spi0
#define PIN_SCK 18
#define PIN_MOSI 19
#define PIN_MISO 20
#define PIN_CS 24
#define PIN_RST 25
#define PIN_EN 8
#define PIN_IRQ 9

// This offset is for the distance between the sensor and the top of the buoy/float
// Set to 0.0f for raw calibration
#define SENSOR_TOP_OFFSET 0.0f

int main() {
    stdio_init_all();
    
    // 1. HARDWARE WAKEUP (Enable Radio/Sensors)
    gpio_init(PIN_EN);
    gpio_set_dir(PIN_EN, GPIO_OUT);
    gpio_put(PIN_EN, 1); 
    sleep_ms(20); 
    
    uint32_t waitTime = 0;
    while (!stdio_usb_connected() && waitTime < 5000) {
        sleep_ms(100);
        waitTime += 100;
    }
    printf("\n--- MS5837 Depth Calibration Utility ---\n");

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
    if (!ms5837_begin(&depth_sensor, I2C_PORT, MS5837_02BA)) {
        printf("CRITICAL ERROR: MS5837 FAILED to initialize\n");
    } else {
        printf("MS5837 Initialized.\n");
    }

    // 4. RADIOLIB SETUP
    printf("Starting RadioLib Setup...\n");
    RadioLibHal_t *hal = RadioLib_Pico_Create(SPI_PORT, PIN_SCK, PIN_MOSI, PIN_MISO, 8000000);
    
    RadioLibModule_t mod;
    memset(&mod, 0, sizeof(RadioLibModule_t));
    RadioLib_Module_Create(&mod, hal, PIN_CS, PIN_IRQ, PIN_RST, RADIOLIB_NC);
    mod.enPin = PIN_EN; 
    
    for (int i = 0; i < 6; i++) {
        mod.radioGPins[i] = RADIOLIB_NC;
    }

    RadioLibSX127x_t lora;
    RadioLib_SX127x_Create(&lora, &mod);

    printf("Attempting RadioLib Begin (915MHz)...\n"); 
    int16_t radio_state = RadioLib_SX1276_Begin(&lora, 915.0, 125.0, 7, 10);
    
    if (radio_state == RADIOLIB_ERR_NONE) {
        printf("Radio Success!\n");
    } else {
        printf("Radio FAILED: %d\n", radio_state);
        // We don't halt here to allow serial debugging of sensor if radio fails
    }

    char tx_buffer[64];
    float depth = 0;

    printf("\nCalibration Loop Starting (1Hz)...\n");
    printf("Place sensor at known depths and record values.\n\n");

    while (true) {
        // A. Update Depth
        ms5837_read(&depth_sensor);
        depth = ms5837_get_depth(&depth_sensor) - SENSOR_TOP_OFFSET;

        // B. Package Data
        snprintf(tx_buffer, sizeof(tx_buffer), "CAL:%.3f", depth);

        // C. Transmit and Print
        if (radio_state == RADIOLIB_ERR_NONE) {
            int16_t tx_state = RadioLib_SX127x_Transmit(&lora, (uint8_t*)tx_buffer, strlen(tx_buffer));
            if (tx_state == RADIOLIB_ERR_NONE) {
                printf("[TX OK] %s\n", tx_buffer);
            } else {
                printf("[TX FAIL %d] %s\n", tx_state, tx_buffer);
            }
        } else {
            printf("[SER] %s\n", tx_buffer);
        }

        sleep_ms(1000); 
    }

    return 0;
}
