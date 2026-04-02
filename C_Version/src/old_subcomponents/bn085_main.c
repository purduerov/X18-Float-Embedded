#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "bno08x_driver.h" // Assuming files are in lib/bno085/

// Hardware configuration
#define I2C_INST i2c1
#define PIN_SDA 2
#define PIN_SCL 3
#define PIN_RESET 15
#define BNO_ADDR BNO08x_I2CADDR_DEFAULT

typedef struct
{
    float accel[3];
    float gyro[3];
    float mag[3];
    float lin_accel[3];
    float gravity[3];
    float quat[4];
    float game_quat[4];
    float geomag_quat[4];
} bno_data_state_t;

void update_state(bno_data_state_t *state, sh2_SensorValue_t *value)
{
    switch (value->sensorId)
    {
    case SH2_ACCELEROMETER:
        state->accel[0] = value->un.accelerometer.x;
        state->accel[1] = value->un.accelerometer.y;
        state->accel[2] = value->un.accelerometer.z;
        break;
    case SH2_GYROSCOPE_CALIBRATED:
        state->gyro[0] = value->un.gyroscope.x;
        state->gyro[1] = value->un.gyroscope.y;
        state->gyro[2] = value->un.gyroscope.z;
        break;
    case SH2_MAGNETIC_FIELD_CALIBRATED:
        state->mag[0] = value->un.magneticField.x;
        state->mag[1] = value->un.magneticField.y;
        state->mag[2] = value->un.magneticField.z;
        break;
    case SH2_LINEAR_ACCELERATION:
        state->lin_accel[0] = value->un.linearAcceleration.x;
        state->lin_accel[1] = value->un.linearAcceleration.y;
        state->lin_accel[2] = value->un.linearAcceleration.z;
        break;
    case SH2_GRAVITY:
        state->gravity[0] = value->un.gravity.x;
        state->gravity[1] = value->un.gravity.y;
        state->gravity[2] = value->un.gravity.z;
        break;
    case SH2_ROTATION_VECTOR:
        state->quat[0] = value->un.rotationVector.i;
        state->quat[1] = value->un.rotationVector.j;
        state->quat[2] = value->un.rotationVector.k;
        state->quat[3] = value->un.rotationVector.real;
        break;
    case SH2_GAME_ROTATION_VECTOR:
        state->game_quat[0] = value->un.gameRotationVector.i;
        state->game_quat[1] = value->un.gameRotationVector.j;
        state->game_quat[2] = value->un.gameRotationVector.k;
        state->game_quat[3] = value->un.gameRotationVector.real;
        break;
    case SH2_GEOMAGNETIC_ROTATION_VECTOR:
        state->geomag_quat[0] = value->un.geoMagRotationVector.i;
        state->geomag_quat[1] = value->un.geoMagRotationVector.j;
        state->geomag_quat[2] = value->un.geoMagRotationVector.k;
        state->geomag_quat[3] = value->un.geoMagRotationVector.real;
        break;
    }
}

void print_inplace(bno_data_state_t *s)
{
    // Clear screen and home cursor
    printf("\033[2J\033[H");
    printf("=== BNO085 COMPLETE SENSOR DATA ===\n\n");
    printf("ACCEL (m/s^2):   X: %7.2f  Y: %7.2f  Z: %7.2f\n", s->accel[0], s->accel[1], s->accel[2]);
    printf("GYRO  (rad/s):   X: %7.2f  Y: %7.2f  Z: %7.2f\n", s->gyro[0], s->gyro[1], s->gyro[2]);
    printf("MAG   (uT):      X: %7.2f  Y: %7.2f  Z: %7.2f\n", s->mag[0], s->mag[1], s->mag[2]);
    printf("LIN ACC (m/s^2): X: %7.2f  Y: %7.2f  Z: %7.2f\n", s->lin_accel[0], s->lin_accel[1], s->lin_accel[2]);
    printf("GRAVITY (m/s^2): X: %7.2f  Y: %7.2f  Z: %7.2f\n", s->gravity[0], s->gravity[1], s->gravity[2]);
    printf("\n--- ROTATION VECTORS (I, J, K, REAL) ---\n");
    printf("STANDARD:        %6.3f, %6.3f, %6.3f, %6.3f\n", s->quat[0], s->quat[1], s->quat[2], s->quat[3]);
    printf("GAME:            %6.3f, %6.3f, %6.3f, %6.3f\n", s->game_quat[0], s->game_quat[1], s->game_quat[2], s->game_quat[3]);
    printf("GEOMAGNETIC:     %6.3f, %6.3f, %6.3f, %6.3f\n", s->geomag_quat[0], s->geomag_quat[1], s->geomag_quat[2], s->geomag_quat[3]);
}

int imu_main()
{
    stdio_init_all();

    i2c_init(I2C_INST, 400 * 1000);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);

    bno08x_driver_t bno;
    bno_data_state_t state;
    memset(&state, 0, sizeof(state));
    
    if (!bno08x_begin_i2c(&bno, I2C_INST, BNO_ADDR, PIN_RESET))
    {
        while (1)
        {
            printf("Init Failed\n");
            sleep_ms(1000);
        }
    }
    bno08x_hardware_reset(&bno);
    uint32_t interval = 50000; // 50ms
    bno08x_enable_report(&bno, SH2_ACCELEROMETER, interval);
    bno08x_enable_report(&bno, SH2_GYROSCOPE_CALIBRATED, interval);
    bno08x_enable_report(&bno, SH2_MAGNETIC_FIELD_CALIBRATED, interval);
    bno08x_enable_report(&bno, SH2_LINEAR_ACCELERATION, interval);
    bno08x_enable_report(&bno, SH2_GRAVITY, interval);
    bno08x_enable_report(&bno, SH2_ROTATION_VECTOR, interval);
    bno08x_enable_report(&bno, SH2_GAME_ROTATION_VECTOR, interval);
    bno08x_enable_report(&bno, SH2_GEOMAGNETIC_ROTATION_VECTOR, interval);

    sh2_SensorValue_t value;
    uint32_t last_print = 0;

    while (1)
    {
        if (bno08x_get_sensor_event(&bno, &value))
        {
            update_state(&state, &value);
        }

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_print > 100)
        {
            print_inplace(&state);
            last_print = now;
        }
    }
}