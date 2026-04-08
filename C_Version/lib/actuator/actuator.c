#include "actuator.h"
#include "hardware/adc.h"
#include <stdio.h>
#include <stdlib.h>

void actuator_init(Actuator *act, uint pos_pin, uint ext_pin, uint ret_pin) {
    act->pos_pin = pos_pin;
    act->ext_pin = ext_pin;
    act->ret_pin = ret_pin;
    act->moving = 0;
    act->move_target = 2048; // Default to mid-range

    adc_init();
    adc_gpio_init(act->pos_pin);

    gpio_init(act->ext_pin);
    gpio_set_dir(act->ext_pin, GPIO_OUT);
    gpio_put(act->ext_pin, false);

    gpio_init(act->ret_pin);
    gpio_set_dir(act->ret_pin, GPIO_OUT);
    gpio_put(act->ret_pin, false);
}

int actuator_get_position(Actuator *act) {
    adc_select_input(act->pos_pin - 26); 
    
    // 256 samples for stability
    uint32_t sum = 0;
    for(int i = 0; i < 256; i++) {
        sum += adc_read();
    }
    
    // Return raw average (0-4095)
    return (int)(sum / 256);
}

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

void actuator_move_to(Actuator *act, int new_position) {
    act->move_target = new_position;
    int current_pos = actuator_get_position(act);
    int direction = (new_position > current_pos) ? 1 : -1;

    if (abs(new_position - current_pos) <= 30) {
        direction = 0; // Within tolerance, stop
    }
    actuator_set_move_pins(act, direction);
}

void actuator_tick(Actuator *act) {
    // Logic handled in main loop
}