#ifndef ACTUATOR_H
#define ACTUATOR_H

#include "pico/stdlib.h"
#include "hw_config.h"

// Constants using raw 12-bit ADC values (0-4095)
#define POS_TOL ACT_POS_TOL
#define ADC_MAX_VAL ACT_ADC_MAX   

typedef struct {
    uint pos_pin;    // ADC pin (e.g., GPIO 26 for A0)
    uint ext_pin;    // Extend motor pin
    uint ret_pin;    // Retract motor pin
    int moving;      // 0: idle, 1: extending, -1: retracting
    int move_target; // Raw ADC target (0-4095)

    // Moving Average Filter
    int pos_history[ACT_FILTER_SIZE];
    int filter_idx;

    // Stall and Timeout Monitoring
    uint32_t move_start_time;
    uint32_t last_pos_time;
    int last_pos;
    bool stalled;
    bool timeout;

    // New fields for recovery
    int retry_count;
    bool hard_locked;
    uint32_t retry_timer;
} Actuator;

/**
 * @brief Initializes the actuator pins and state.
 */
void actuator_init(Actuator *act, uint pos_pin, uint ext_pin, uint ret_pin);

/**
 * @brief Reads the noise-filtered ADC position.
 */
int actuator_get_position(Actuator *act);

/**
 * @brief Directly sets the motor driver pins.
 * @param direction 1: extend, -1: retract, 0: stop
 */
void actuator_set_move_pins(Actuator *act, int direction);

/**
 * @brief Sets a target position and begins a non-blocking move.
 * Call actuator_tick() periodically to monitor progress and handle stalls.
 */
void actuator_move_to(Actuator *act, int new_position);

/**
 * @brief Monitors the move progress, handles stall detection and timeouts.
 * Should be called periodically in the main loop (~10-100Hz).
 */
void actuator_tick(Actuator *act);

/**
 * @brief Initializes PWM-based VREF control for the motor driver.
 */
void actuator_vref_init(void);

/**
 * @brief Sets the motor driver VREF (power level) based on PID output.
 * @param pid_output Raw PID output value to map to VREF duty cycle.
 */
void actuator_vref_set(double pid_output);

#endif // ACTUATOR_H