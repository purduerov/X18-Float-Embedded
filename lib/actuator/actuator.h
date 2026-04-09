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
} Actuator;

// Function Prototypes
void actuator_init(Actuator *act, uint pos_pin, uint ext_pin, uint ret_pin);
int actuator_get_position(Actuator *act);
void actuator_set_move_pins(Actuator *act, int direction);
void actuator_move_to(Actuator *act, int new_position);
void actuator_tick(Actuator *act);

#endif // ACTUATOR_H