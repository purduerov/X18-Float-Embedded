#include "pid.h"
#include <stdbool.h>

void pid_init(PIDController *pid, double kp, double ki, double kd, double dt, double output_min, double output_max) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->dt = dt;
    pid->integral = 0.0;
    pid->prev_error = 0.0;
    pid->output_min = output_min;
    pid->output_max = output_max;
    pid->integral_min = output_min;
    pid->integral_max = output_max;
    pid->integral_gate = 0.0; // Disabled by default
    pid->prev_D = 0.0;
}

void pid_update(PIDController *pid, double error, double *output) {
    double P = pid->kp * error;

    // One-Way Conditional Integration (Gating)
    // We only gate the integral when we are TOO SHALLOW (descending).
    // This prevents windup during the long drop, but allows the PID to 
    // build maximum buoyancy if we ever go too deep (overshoot).
    bool accumulate = true;
    if (pid->integral_gate > 0.0) {
        if (error < -pid->integral_gate) { // Too shallow by more than threshold
            accumulate = false;
        }
    }

    if (accumulate) {
        pid->integral += error * pid->dt;
    }

    // Apply integral windup bound (Anti-windup)
    // We limit the integral term's CONTRIBUTION to the specified limits.
    if (pid->ki != 0) {
        double i_term_max = pid->integral_max;
        double i_term_min = pid->integral_min;

        if (pid->ki * pid->integral > i_term_max) pid->integral = i_term_max / pid->ki;
        if (pid->ki * pid->integral < i_term_min) pid->integral = i_term_min / pid->ki;
    }

    double I = pid->ki * pid->integral;
    
    // Derivative calculation with EMA Filtering
    // Alpha (0.3) weights the newest measurement; (0.7) weights the historical state.
    // This damps rapid fluctuations from sensor noise/bubbles.
    double raw_D = pid->kd * ((error - pid->prev_error) / pid->dt);
    double D = (0.3 * raw_D) + (0.7 * pid->prev_D);
    pid->prev_D = D;

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
    pid->prev_D = 0.0;
}

void pid_set_integral(PIDController *pid, double integral_value) {
    // We allow setting the integral directly. 
    // This is useful for "seeding" the PID with a baseline guess (like Neutral ADC)
    // and for anti-windup checks.
    if (pid->ki != 0) {
        pid->integral = integral_value / pid->ki;
    } else {
        pid->integral = 0;
    }
}