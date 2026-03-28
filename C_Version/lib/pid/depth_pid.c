#include "depth_pid.h"

void depth_pid_init(DepthPID *dpid, double kp, double ki, double kd, double dt, int pos_min, int pos_max) {
    // Initialize underlying PID controller.
    // We map the output range directly to the actuator's ADC range.
    pid_init(&dpid->pid, kp, ki, kd, dt, (double)pos_min, (double)pos_max);
    
    dpid->target_depth = 0.0;
    dpid->pos_min = pos_min;
    dpid->pos_max = pos_max;
}

void depth_pid_set_target(DepthPID *dpid, double target_depth) {
    dpid->target_depth = target_depth;
}

void depth_pid_calculate_target_pos(DepthPID *dpid, double current_depth, int *target_actuator_pos) {
    /**
     * Error Calculation:
     * If current_depth > target_depth (too deep), error is positive.
     * A positive error will result in a positive PID output (assuming positive gains).
     * We map positive output to higher actuator position (pos_max), 
     * which increases buoyancy to bring the float back up.
     */
    double error = current_depth - dpid->target_depth;
    double output = 0;
    
    pid_update(&dpid->pid, error, &output);
    
    // The PID output is already clamped to [pos_min, pos_max] by pid_update
    *target_actuator_pos = (int)output;
}
