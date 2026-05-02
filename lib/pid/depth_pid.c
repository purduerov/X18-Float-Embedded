#include "depth_pid.h"

void depth_pid_init(DepthPID *dpid, double kp, double ki, double kd, double dt, int pos_min, int pos_max) {
    // Initialize underlying PID controller.
    // The PID output is an offset from neutral, so its limits should cover the full possible range.
    // For a 0-4095 actuator, the maximum possible offset is +/- 4095.
    pid_init(&dpid->pid, kp, ki, kd, dt, -4095.0, 4095.0);
    
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

void depth_pid_calculate_target_pos(DepthPID *dpid, double current_depth, int neutral_adc, int *target_actuator_pos) {
    /**
     * ABSOLUTE POSITIONAL CONTROL:
     * Instead of calculating a relative adjustment, we calculate an absolute
     * position centered on the "Neutral Buoyancy" ADC baseline.
     * 
     * Formula:
     * Target_Position = Neutral_ADC + PID_Output
     * 
     * Direction:
     * 4095 = UP (Maximum Buoyancy / Expansion)
     * 0    = DOWN (Minimum Buoyancy / Retraction)
     * 
     * Error Logic (current_depth - target_depth):
     * If current_depth > target_depth (TOO DEEP):
     *   - Error is positive.
     *   - PID output should be positive to increase buoyancy (move UP toward 4095).
     */
    double error = current_depth - dpid->target_depth;
    double pid_output = 0;
    
    // Calculate the absolute offset from neutral using the PID.
    // The PID limits in the underlying controller should be +/- 2048 or similar
    // to allow the output to cover the full actuator range around the neutral point.
    pid_update(&dpid->pid, error, &pid_output);
    
    // Apply the output to the baseline
    int new_pos = neutral_adc + (int)pid_output;
    
    // Clamp to hardware limits (0-4095 or as defined by settings)
    if (new_pos > dpid->pos_max) new_pos = dpid->pos_max;
    if (new_pos < dpid->pos_min) new_pos = dpid->pos_min;
    
    *target_actuator_pos = new_pos;
}
