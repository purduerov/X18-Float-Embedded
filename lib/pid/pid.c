#include "pid.h"

void pid_init(PIDController *pid, double kp, double ki, double kd, double dt, double output_min, double output_max) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->dt = dt;
    pid->integral = 0.0;
    pid->prev_error = 0.0;
    pid->output_min = output_min;
    pid->output_max = output_max;
}

void pid_update(PIDController *pid, double error, double *output) {
    double P = pid->kp * error;

    // Calculate potential integral update
    pid->integral += error * pid->dt;

    // Apply integral windup bound (Anti-windup)
    // We limit the integral term's CONTRIBUTION to the total output limits.
    // This prevents the I-term from saturating the actuator on its own.
    if (pid->ki != 0) {
        double i_term_max = pid->output_max;
        double i_term_min = pid->output_min;

        if (pid->ki * pid->integral > i_term_max) pid->integral = i_term_max / pid->ki;
        if (pid->ki * pid->integral < i_term_min) pid->integral = i_term_min / pid->ki;
    }

    double I = pid->ki * pid->integral;
    double D = pid->kd * ((error - pid->prev_error) / pid->dt);
    double raw_output = P + I + D;

    // Clamp total output to limits
    if (raw_output > pid->output_max) raw_output = pid->output_max;
    else if (raw_output < pid->output_min) raw_output = pid->output_min;

    pid->prev_error = error;
    *output = raw_output;
}

void pid_reset(PIDController *pid) {
    pid->integral = 0.0;
    pid->prev_error = 0.0;
}