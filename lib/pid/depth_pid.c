#include "depth_pid.h"
#include <math.h>

void depth_pid_init(DepthPID *dpid, double kp, double ki, double kd, double dt, int pos_min, int pos_max) {
    // Initialize underlying PID controller.
    // Allow total output to span the full physical/soft limits (usually 0 to 4095)
    double range = (double)(pos_max - pos_min);
    pid_init(&dpid->pid, kp, ki, kd, dt, -range, range);

    dpid->target_depth = 0.0;
    dpid->pos_min = pos_min;
    dpid->pos_max = pos_max;
}

void depth_pid_set_target(DepthPID *dpid, double target_depth) {
    dpid->target_depth = target_depth;
}

void depth_pid_reset(DepthPID *dpid, double current_error) {
    pid_reset(&dpid->pid, (float)current_error);
}

void depth_pid_calculate_target_pos(DepthPID *dpid, double current_depth, int neutral_adc, int *target_actuator_pos) {
    float error = (float)(current_depth - dpid->target_depth);
    float pid_output = 0;

    pid_update(&dpid->pid, error, &pid_output);

    int new_pos = neutral_adc + (int)roundf(pid_output);
    
    // Clamp to hardware limits (0-4095 or as defined by settings)
    if (new_pos > dpid->pos_max) new_pos = dpid->pos_max;
    if (new_pos < dpid->pos_min) new_pos = dpid->pos_min;
    
    *target_actuator_pos = new_pos;
}
