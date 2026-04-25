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

void depth_pid_reset(DepthPID *dpid) {
    pid_reset(&dpid->pid);
}

void depth_pid_calculate_target_pos(DepthPID *dpid, double current_depth, int *target_actuator_pos) {
    /**
     * INCREMENTAL (VELOCITY) CONTROL:
     * Instead of calculating the absolute position, we calculate a small 
     * ADJUSTMENT to the current position. 
     * 
     * Direction:
     * 4095 = UP (Maximum Buoyancy)
     * 0    = DOWN (Minimum Buoyancy)
     * 
     * If current_depth > target_depth (TOO DEEP):
     *   - Error is positive (current - target)
     *   - PID output should be positive to increase buoyancy (move UP toward 4095)
     */
    double error = current_depth - dpid->target_depth;
    double adjustment = 0;
    
    // We use the underlying PID to calculate the step size.
    // The PID output limits (min/max) now represent the max change per tick.
    pid_update(&dpid->pid, error, &adjustment);
    
    // Apply the adjustment to the existing position
    int new_pos = *target_actuator_pos + (int)adjustment;
    
    // Clamp to hardware limits
    if (new_pos > dpid->pos_max) new_pos = dpid->pos_max;
    if (new_pos < dpid->pos_min) new_pos = dpid->pos_min;
    
    *target_actuator_pos = new_pos;
}
