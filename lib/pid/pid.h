#ifndef PID_H
#define PID_H

typedef struct {
    double kp;
    double ki;
    double kd;
    double dt;
    double integral;
    double prev_error;
    double output_min;
    double output_max;
    double integral_gate; // Conditional integration threshold (0 to disable)
    double prev_D;        // Previous filtered derivative state for EMA
} PIDController;

void pid_init(PIDController *pid, double kp, double ki, double kd, double dt, double output_min, double output_max);
void pid_update(PIDController *pid, double error, double *output);
void pid_reset(PIDController *pid);

#endif