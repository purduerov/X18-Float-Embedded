#ifndef DEPTH_PID_H
#define DEPTH_PID_H

#include "pid.h"

/**
 * @brief Wrapper for the PID controller specifically for depth maintenance.
 * Maps depth error (meters) to target actuator position (0-4095).
 */
typedef struct {
    PIDController pid;
    double target_depth;
    int pos_min; // Minimum actuator position (usually 0)
    int pos_max; // Maximum actuator position (usually 4095)
} DepthPID;

/**
 * @brief Initializes the depth PID controller.
 * @param dpid Pointer to DepthPID structure.
 * @param kp Proportional gain.
 * @param ki Integral gain.
 * @param kd Derivative gain.
 * @param dt Time step (seconds).
 * @param pos_min Minimum actuator position.
 * @param pos_max Maximum actuator position.
 */
void depth_pid_init(DepthPID *dpid, double kp, double ki, double kd, double dt, int pos_min, int pos_max);

/**
 * @brief Sets the target depth.
 * @param dpid Pointer to DepthPID structure.
 * @param target_depth Target depth in meters.
 */
void depth_pid_set_target(DepthPID *dpid, double target_depth);

/**
 * @brief Resets the underlying PID integral and error terms.
 * @param dpid Pointer to DepthPID structure.
 */
void depth_pid_reset(DepthPID *dpid);

/**
 * @brief Calculates the target actuator position based on current depth.
 * @param dpid Pointer to DepthPID structure.
 * @param current_depth Current depth in meters from sensor.
 * @param neutral_adc The ADC baseline for neutral buoyancy.
 * @param target_actuator_pos Output: The calculated target position for the actuator (0-4095).
 */
void depth_pid_calculate_target_pos(DepthPID *dpid, double current_depth, int neutral_adc, int *target_actuator_pos);

#endif // DEPTH_PID_H
