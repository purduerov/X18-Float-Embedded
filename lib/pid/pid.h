#ifndef PID_H
#define PID_H

typedef struct {
    float kp;
    float ki;
    float kd;
    float dt;
    float integral;
    float prev_error;
    float output_min;
    float output_max;
} PIDController;

void pid_init(PIDController *pid, float kp, float ki, float kd, float dt, float output_min, float output_max);
void pid_update(PIDController *pid, float error, float *output);
void pid_reset(PIDController *pid, float current_error);
void pid_set_integral(PIDController *pid, float integral_value);

#endif