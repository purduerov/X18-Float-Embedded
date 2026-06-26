#include "pid.h"

void pid_init(PIDController *pid, float kp, float ki, float kd, float dt, float output_min, float output_max) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->dt = dt;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->output_min = output_min;
    pid->output_max = output_max;
}

void pid_update(PIDController *pid, float error, float *output) {
    float P = pid->kp * error;

    pid->integral += error * pid->dt;
    float I = pid->ki * pid->integral;

    float D = pid->kd * ((error - pid->prev_error) / pid->dt);

    float raw_output = P + I + D;

    // Clamp total output to limits
    if (raw_output > pid->output_max) raw_output = pid->output_max;
    else if (raw_output < pid->output_min) raw_output = pid->output_min;

    pid->prev_error = error;
    *output = raw_output;
}

void pid_reset(PIDController *pid, float current_error) {
    pid->integral = 0.0f;
    pid->prev_error = current_error;
}

void pid_set_integral(PIDController *pid, float integral_value) {
    // We allow setting the integral directly.
    // This is useful for "seeding" the PID with a baseline guess (like Neutral ADC)
    // and for anti-windup checks.
    if (pid->ki != 0.0f) {
        pid->integral = integral_value / pid->ki;
    } else {
        pid->integral = 0.0f;
    }
}