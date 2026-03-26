#ifndef __MS5837_H_
#define __MS5837_H_

#include <stdint.h>
#include <stdbool.h>

#define MS5837_ADDR 0x76
#define MS5837_30BA 0
#define MS5837_02BA 1
#define MS5837_UNRECOGNISED 255
#define Pa 100.0f
#define bar 0.001f
#define mbar 1.0f

typedef struct {
    // Hardware Reference
    void* i2c_inst;      // Pointer to your I2C hardware instance

    // Calibration Data
    uint16_t C[8];       // PROM calibration coefficients
    
    // Raw and Processed Values
    uint32_t D1;         // Raw pressure
    uint32_t D2;         // Raw temperature
    int32_t TEMP;        // Calculated temperature (centidegrees)
    int32_t P;           // Calculated pressure

    // Settings
    uint8_t model;       // Sensor model (02BA or 30BA)
    float fluidDensity;  // Used for depth calculations
} MS5837_t;


void ms5837_init_struct(MS5837_t *sensor);
bool ms5837_begin(MS5837_t *sensor, void *i2c_inst, uint8_t forced_model);
void ms5837_read(MS5837_t *sensor);
void ms5837_calculate(MS5837_t *sensor);

float ms5837_get_pressure(MS5837_t *sensor, float conversion);
float ms5837_get_temperature(MS5837_t *sensor);
float ms5837_get_depth(MS5837_t *sensor);
float ms5837_get_altitude(MS5837_t *sensor);

#endif