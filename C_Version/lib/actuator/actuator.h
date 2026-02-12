#ifndef ACTUATOR_H
#define ACTUATOR_H

#include "pico/stdlib.h"

// Constants derived from the Python implementation
#define POS_TOL 0.01f
#define ADC_MAX 4095.0f // RP2040 ADC is 12-bit

typedef struct {
    uint pos_pin;    // ADC pin (e.g., GPIO 26 for A0)
    uint ext_pin;    // Extend motor pin
    uint ret_pin;    // Retract motor pin
    int moving;      // 0: idle, 1: extending, -1: retracting
    float move_target;
} Actuator;

// Function Prototypes
void actuator_init(Actuator *act, uint pos_pin, uint ext_pin, uint ret_pin);
float actuator_get_position(Actuator *act);
void actuator_set_move_pins(Actuator *act, int direction);
void actuator_move_to(Actuator *act, float new_position);
void actuator_tick(Actuator *act);

#endif // ACTUATOR_H