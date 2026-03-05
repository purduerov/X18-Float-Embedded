#include "ms5837.h"
#include <math.h>
#include "hardware/i2c.h"
#include <stdio.h> // Required for debug prints

// These constants define the commands sent over I2C to control the sensor.
#define MS5837_RESET_CMD 0x1E       // Command to reset the sensor.
#define MS5837_ADC_READ 0x00        // Command to read the ADC result.
#define MS5837_PROM_READ 0xA0       // Base command to read calibration words.
#define MS5837_CONVERT_D1_8192 0x4A // Pressure conversion with max resolution.
#define MS5837_CONVERT_D2_8192 0x5A // Temperature conversion with max resolution.

// These thresholds are used to differentiate between the 30BA and 02BA models.
#define MS5837_02BA_MAX_SENSITIVITY 49000
#define MS5837_02BA_30BA_SEPARATION 37000
#define MS5837_30BA_MIN_SENSITIVITY 26000

static uint8_t ms5837_crc4(uint16_t n_prom[])
{
    uint16_t n_rem = 0;

    // We must mask out the CRC bits (first 4 bits of word 0) before calculating
    n_prom[0] = ((n_prom[0]) & 0x0FFF);
    n_prom[7] = 0;

    for (uint8_t i = 0; i < 16; i++)
    {
        if (i % 2 == 1)
        {
            n_rem ^= (uint16_t)((n_prom[i >> 1]) & 0x00FF);
        }
        else
        {
            n_rem ^= (uint16_t)(n_prom[i >> 1] >> 8);
        }
        for (uint8_t n_bit = 8; n_bit > 0; n_bit--)
        {
            if (n_rem & 0x8000)
            {
                n_rem = (n_rem << 1) ^ 0x3000;
            }
            else
            {
                n_rem = (n_rem << 1);
            }
        }
    }

    return ((n_rem >> 12) & 0x000F);
}

void ms5837_init_struct(MS5837_t *sensor)
{
    // We set the default fluid density to seawater.
    // The original library uses 1029 kg/m^3 as the default.
    sensor->fluidDensity = 1029.0f;

    // We initialize the I2C pointer to NULL to ensure we don't
    // try to use it before ms5837_begin is called.
    sensor->i2c_inst = NULL;

    // It is good practice to zero out the calibration array (C)
    // and raw data (D1, D2) so we don't have garbage data.
    for (int i = 0; i < 8; i++)
    {
        sensor->C[i] = 0;
    }
    sensor->D1 = 0;
    sensor->D2 = 0;
    sensor->TEMP = 0;
    sensor->P = 0;
    sensor->model = MS5837_UNRECOGNISED;
}

// Inside ms5837.c

bool ms5837_begin(MS5837_t *sensor, void *i2c_inst, uint8_t forced_model)
{
    sensor->i2c_inst = i2c_inst;

    // Reset the sensor
    uint8_t reset_cmd = MS5837_RESET_CMD;
    i2c_write_timeout_us(sensor->i2c_inst, MS5837_ADDR, &reset_cmd, 1, false, 100000);
    sleep_ms(100);

    // Read PROM coefficients
    for (uint8_t i = 0; i < 7; i++)
    {
        uint8_t prom_read_cmd = MS5837_PROM_READ + (i * 2);
        i2c_write_timeout_us(sensor->i2c_inst, MS5837_ADDR, &prom_read_cmd, 1, true, 100000);

        uint8_t buffer[2];
        int result = i2c_read_timeout_us(sensor->i2c_inst, MS5837_ADDR, buffer, 2, false, 100000);

        if (result >= 0) {
            sensor->C[i] = (buffer[0] << 8) | buffer[1];
        }
    }

    // Logic for setting the model
    if (forced_model != MS5837_UNRECOGNISED) {
        sensor->model = forced_model;
        printf("MS5837: Model manually set to %s\n", (sensor->model == MS5837_02BA) ? "02BA" : "30BA");
    } else {
        // Fallback to auto-detection logic
        sensor->model = (sensor->C[1] > 37000) ? MS5837_02BA : MS5837_30BA;
        printf("MS5837: Auto-detected model %s\n", (sensor->model == MS5837_02BA) ? "02BA" : "30BA");
    }

    return true;
}

void ms5837_read(MS5837_t *sensor)
{
    uint8_t cmd;
    uint8_t buffer[3];

    // Request D1 (Pressure) conversion
    cmd = 0x4A; // MS5837_CONVERT_D1_8192
    i2c_write_blocking(sensor->i2c_inst, MS5837_ADDR, &cmd, 1, false);
    sleep_ms(20);

    // Read D1 ADC
    cmd = 0x00; // MS5837_ADC_READ
    i2c_write_blocking(sensor->i2c_inst, MS5837_ADDR, &cmd, 1, false);
    i2c_read_blocking(sensor->i2c_inst, MS5837_ADDR, buffer, 3, false);
    sensor->D1 = ((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | buffer[2];

    // Request D2 (Temperature) conversion
    cmd = 0x5A; // MS5837_CONVERT_D2_8192
    i2c_write_blocking(sensor->i2c_inst, MS5837_ADDR, &cmd, 1, false);
    sleep_ms(20);

    // Read D2 ADC
    cmd = 0x00;
    i2c_write_blocking(sensor->i2c_inst, MS5837_ADDR, &cmd, 1, false);
    i2c_read_blocking(sensor->i2c_inst, MS5837_ADDR, buffer, 3, false);
    sensor->D2 = ((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | buffer[2];

    ms5837_calculate(sensor);
}

void ms5837_calculate(MS5837_t *sensor)
{
    // We define local variables for intermediate calculation steps.
    // Using int64_t is mandatory here to prevent overflow.
    int32_t dT = 0;
    int64_t SENS = 0;
    int64_t OFF = 0;
    int32_t SENSi = 0;
    int32_t OFFi = 0;
    int32_t Ti = 0;
    int64_t OFF2 = 0;
    int64_t SENS2 = 0;

    // --- Part 1: First Order Compensation ---

    // Calculate the difference between actual and reference temperature.
    // Formula: dT = D2 - T_REF = D2 - C5 * 2^8
    dT = sensor->D2 - (uint32_t)sensor->C[5] * 256L;

    // Calculate SENS (Sensitivity) and OFF (Offset) based on the sensor model.
    if (sensor->model == 1)
    { // MS5837_02BA
        SENS = (int64_t)sensor->C[1] * 65536L + ((int64_t)sensor->C[3] * dT) / 128L;
        OFF = (int64_t)sensor->C[2] * 131072L + ((int64_t)sensor->C[4] * dT) / 64L;
    }
    else
    { // MS5837_30BA
        SENS = (int64_t)sensor->C[1] * 32768L + ((int64_t)sensor->C[3] * dT) / 256L;
        OFF = (int64_t)sensor->C[2] * 65536L + ((int64_t)sensor->C[4] * dT) / 128L;
    }

    // Calculate the initial temperature in centidegrees (2000 = 20.00°C).
    sensor->TEMP = 2000L + (int64_t)dT * sensor->C[6] / 8388608LL;

    // --- Part 2: Second Order Temperature Compensation ---
    // This part corrects for non-linearities at low and high temperatures.

    if (sensor->model == 1)
    { // MS5837_02BA
        if ((sensor->TEMP / 100) < 20)
        { // Low temperature (< 20°C)
            Ti = (11 * (int64_t)dT * dT) / 34359738368LL;
            OFFi = (31 * (sensor->TEMP - 2000) * (sensor->TEMP - 2000)) / 8;
            SENSi = (63 * (sensor->TEMP - 2000) * (sensor->TEMP - 2000)) / 32;
        }
    }
    else
    { // MS5837_30BA
        if ((sensor->TEMP / 100) < 20)
        { // Low temperature (< 20°C)
            Ti = (3 * (int64_t)dT * dT) / 8589934592LL;
            OFFi = (3 * (sensor->TEMP - 2000) * (sensor->TEMP - 2000)) / 2;
            SENSi = (5 * (sensor->TEMP - 2000) * (sensor->TEMP - 2000)) / 8;

            if ((sensor->TEMP / 100) < -15)
            { // Very low temperature (< -15°C)
                OFFi = OFFi + 7 * (sensor->TEMP + 1500L) * (sensor->TEMP + 1500L);
                SENSi = SENSi + 4 * (sensor->TEMP + 1500L) * (sensor->TEMP + 1500L);
            }
        }
        else if ((sensor->TEMP / 100) >= 20)
        { // High temperature (> 20°C)
            Ti = 2 * ((int64_t)dT * dT) / 137438953472LL;
            OFFi = (1 * (sensor->TEMP - 2000) * (sensor->TEMP - 2000)) / 16;
            SENSi = 0;
        }
    }

    // Apply the second-order offsets to the base values.
    OFF2 = OFF - OFFi;
    SENS2 = SENS - SENSi;

    // Update the final temperature and pressure in the struct.
    sensor->TEMP = sensor->TEMP - Ti;

    if (sensor->model == 1)
    { // MS5837_02BA
        sensor->P = (((sensor->D1 * SENS2) / 2097152L - OFF2) / 32768L);
    }
    else
    { // MS5837_30BA
        sensor->P = (((sensor->D1 * SENS2) / 2097152L - OFF2) / 8192L);
    }
}

float ms5837_get_pressure(MS5837_t *sensor, float conversion)
{
    if (sensor->model == MS5837_02BA)
    {
        return (float)sensor->P * conversion / 100.0f;
    }
    else
    {
        // For 30BA, the pressure calculation results in 0.1 mbar units
        return (float)sensor->P * conversion / 10.0f;
    }
}

float ms5837_get_temperature(MS5837_t *sensor)
{
    // TEMP is calculated in centidegrees (100 * deg C)
    return (float)sensor->TEMP / 100.0f;
}

float ms5837_get_depth(MS5837_t *sensor)
{
    // Uses the standard atmospheric pressure of 101300 Pa as a baseline
    return (ms5837_get_pressure(sensor, Pa) - 101300.0f) / (sensor->fluidDensity * 9.80665f);
}

float ms5837_get_altitude(MS5837_t *sensor)
{
    // Standard altitude formula using 1013.25 mbar as sea level pressure
    return (1.0f - powf((ms5837_get_pressure(sensor, 1.0f) / 1013.25f), 0.190284f)) * 145366.45f * 0.3048f;
}