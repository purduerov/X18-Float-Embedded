#include "depth_pid.h"
#include "sw_config.h"

void depth_pid_init(DepthPID *dpid, double kp, double ki, double kd, double dt, int pos_min, int pos_max) {
    // Initialize underlying PID controller.
    // ASYMMETRIC LIMITS:
    // Max (+4095): Full positive authority to surface or stop plunges.
    // Min (-800): Capped negative authority. Allows the float to reach a heavy state
    // to sink reliably, but prevents the massive lead-weight plunge.
    pid_init(&dpid->pid, kp, ki, kd, dt, -800.0, 4095.0);
    
    // Enable Integral Gating to prevent windup during the descent.
    dpid->pid.integral_gate = (double)INTEGRAL_GATE_M;
    
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
     * 
     * Formula: Target_Position = Neutral_ADC + PID_Output
     * 
     * Direction:
     * 4095 = UP (Maximum Buoyancy / Expansion)
     * 0    = DOWN (Minimum Buoyancy / Retraction)
     */
    double error = current_depth - dpid->target_depth;
    double pid_output = 0;
    
    pid_update(&dpid->pid, error, &pid_output);
    
    // Apply the output to the baseline
    int new_pos = neutral_adc + (int)pid_output;
    
    // Clamp to hardware limits (0-4095 or as defined by settings)
    if (new_pos > dpid->pos_max) new_pos = dpid->pos_max;
    if (new_pos < dpid->pos_min) new_pos = dpid->pos_min;
    
    *target_actuator_pos = new_pos;
}
