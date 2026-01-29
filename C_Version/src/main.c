#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ms5837.h"

// I2C hardware configuration for Raspberry Pi Pico
#define I2C_PORT i2c1
#define SDA_PIN 2
#define SCL_PIN 3

int main()
{
    // Initialize standard I/O for serial communication via USB or UART
    stdio_init_all();

    // Initialize I2C at 400kHz
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

    // Enable internal pull-ups for I2C lines
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    MS5837_t sensor;
    ms5837_init_struct(&sensor);

    printf("Starting\n");

    // Initialize pressure sensor
    // The program loops until the sensor is successfully identified on the I2C bus
    while (!ms5837_begin(&sensor, I2C_PORT))
    {
        printf("Init failed!\n");
        printf("Are SDA/SCL connected correctly?\n");
        printf("Blue Robotics Bar30: White=SDA, Green=SCL\n");
        printf("\n\n\n");
        sleep_ms(5000);
    }

    // The begin function automatically detects the sensor model, but it can be overridden
    sensor.model = MS5837_02BA;

    // Set fluid density to freshwater (997 kg/m^3)
    // Seawater density is typically 1029 kg/m^3
    sensor.fluidDensity = 997.0f;

    while (true)
    {
        // Update pressure and temperature readings from the sensor
        ms5837_read(&sensor);

        // Output the results to the serial terminal
        printf("Pressure: %.2f mbar\n", ms5837_get_pressure(&sensor, 1.0f));
        printf("Temperature: %.2f deg C\n", ms5837_get_temperature(&sensor));
        printf("Depth: %.2f m\n", ms5837_get_depth(&sensor));
        printf("Altitude: %.2f m above mean sea level\n", ms5837_get_altitude(&sensor));

        // // This bypasses floating point formatter issues
        // printf("Raw Temp: %ld\n", sensor.TEMP);
        // printf("Raw Pressure: %ld\n", sensor.P);

        // // Or print as "fixed point" (temp is in centidegrees)
        // printf("Temperature: %ld.%02ld deg C\n", sensor.TEMP / 100, sensor.TEMP % 100);

        // Wait for 1 second before the next measurement loop
        sleep_ms(1000);
    }

    return 0;
}