#include "actuator.h"
#include "hardware/adc.h"
#include <math.h>
#include <stdio.h>

/**
 * Initializes hardware and the actuator structure.
 * Replaces the Python __init__ method.
 */
void actuator_init(Actuator *act, uint pos_pin, uint ext_pin, uint ret_pin) {
    act->pos_pin = pos_pin;
    act->ext_pin = ext_pin;
    act->ret_pin = ret_pin;
    act->moving = 0;
    act->move_target = 0.5f;

    // Initialize ADC for position feedback
    adc_init();
    adc_gpio_init(act->pos_pin);

    // Initialize GPIOs for motor control
    gpio_init(act->ext_pin);
    gpio_set_dir(act->ext_pin, GPIO_OUT);
    gpio_put(act->ext_pin, false);

    gpio_init(act->ret_pin);
    gpio_set_dir(act->ret_pin, GPIO_OUT);
    gpio_put(act->ret_pin, false);
    
    printf("Linear actuator initialized\n");
}

/**
 * Reads the current position from the potentiometer.
 * Scales the 12-bit ADC value to a 0.0 to 1.0 range.
 */
float actuator_get_position(Actuator *act) {
    // Determine ADC channel from GPIO pin (GPIO 26 = Channel 0)
    adc_select_input(act->pos_pin - 26); 
    uint16_t raw = adc_read();
    return (float)raw / ADC_MAX;
}

/**
 * Updates the physical GPIO pins based on direction.
 * Equivalent to set_move_pins in Python.
 */
void actuator_set_move_pins(Actuator *act, int direction) {
    act->moving = direction;
    if (direction == 1) {
        gpio_put(act->ext_pin, true);
        gpio_put(act->ret_pin, false);
    } else if (direction == -1) {
        gpio_put(act->ext_pin, false);
        gpio_put(act->ret_pin, true);
    } else {
        gpio_put(act->ext_pin, false);
        gpio_put(act->ret_pin, false);
    }
}

/**
 * Initiates movement to a specific position.
 * Implements the logic from the Python move_to method.
 */
void actuator_move_to(Actuator *act, float new_position) {
    act->move_target = new_position;
    float current_pos = actuator_get_position(act);

    if (fabsf(new_position - current_pos) < POS_TOL) {
        actuator_set_move_pins(act, 0);
        return;
    }

    int direction = (new_position > current_pos) ? 1 : -1;
    actuator_set_move_pins(act, direction);
}

/**
 * Periodic update function to check if the target has been reached.
 * Should be called by your scheduler.
 */
void actuator_tick(Actuator *act) {
    if (act->moving == 0) return;

    float current_pos = actuator_get_position(act);
    
    // Check if we have reached or exceeded the target based on direction
    bool reached = false;
    if (act->moving == 1 && current_pos >= act->move_target) reached = true;
    if (act->moving == -1 && current_pos <= act->move_target) reached = true;

    if (reached) {
        actuator_set_move_pins(act, 0);
    }
}