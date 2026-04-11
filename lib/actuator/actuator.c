#include "actuator.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Dynamic VREF Settings (PWM duty cycle)
#define VREF_PWM_WRAP 65535
#define VREF_MIN_DUTY 19859 // ~30% power (minimum to move under load)
#define VREF_MAX_DUTY 65535 // 100% power

void actuator_init(Actuator *act, uint pos_pin, uint ext_pin, uint ret_pin) {
    act->pos_pin = pos_pin;
    act->ext_pin = ext_pin;
    act->ret_pin = ret_pin;
    act->moving = 0;
    act->move_target = DEFAULT_ACTUATOR_POS;

    // Reset filtering history
    act->filter_idx = 0;
    memset(act->pos_history, 0, sizeof(act->pos_history));

    // Reset monitoring state
    act->move_start_time = 0;
    act->last_pos_time = 0;
    act->last_pos = 0;
    act->stalled = false;
    act->timeout = false;

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
    uint32_t sum_samples = 0;
    for(int i = 0; i < 256; i++) {
        sum_samples += adc_read();
    }
    int raw_pos = (int)(sum_samples / 256);

    // Apply moving average filter
    act->pos_history[act->filter_idx] = raw_pos;
    act->filter_idx = (act->filter_idx + 1) % ACT_FILTER_SIZE;

    long sum_filter = 0;
    for (int i = 0; i < ACT_FILTER_SIZE; i++) {
        sum_filter += act->pos_history[i];
    }
    return (int)(sum_filter / ACT_FILTER_SIZE);
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
        act->move_start_time = 0; // Reset monitoring
        act->last_pos_time = 0;
    }
}

void actuator_move_to(Actuator *act, int new_position) {
    // If target changed, reset stall/timeout timers
    if (act->move_target != new_position) {
        act->move_target = new_position;
        act->move_start_time = to_ms_since_boot(get_absolute_time());
        act->last_pos_time = act->move_start_time;
        act->last_pos = actuator_get_position(act);
        act->stalled = false;
        act->timeout = false;
    }

    int current_pos = actuator_get_position(act);
    int direction = (new_position > current_pos) ? 1 : -1;

    if (abs(new_position - current_pos) <= POS_TOL) {
        direction = 0; // Within tolerance, stop
    }
    
    actuator_set_move_pins(act, direction);
}

void actuator_tick(Actuator *act) {
    if (act->moving == 0) return;

    uint32_t t_now = to_ms_since_boot(get_absolute_time());
    int current_pos = actuator_get_position(act);

    // Initial timer setup if move_to wasn't used or reset
    if (act->move_start_time == 0) {
        act->move_start_time = t_now;
        act->last_pos_time = t_now;
        act->last_pos = current_pos;
    }

    // Check for movement progress (reset stall timer)
    if (abs(current_pos - act->last_pos) >= ACT_STALL_THRESHOLD) {
        act->last_pos = current_pos;
        act->last_pos_time = t_now;
    }

    // Check for Stall
    if (t_now - act->last_pos_time > ACT_STALL_MS) {
        printf("!! [ACTUATOR] STALL DETECTED at %d. Stopping.\n", current_pos);
        actuator_set_move_pins(act, 0);
        act->stalled = true;
    } 
    // Check for Timeout
    else if (t_now - act->move_start_time > ACT_MOVE_TIMEOUT_MS) {
        printf("!! [ACTUATOR] MOVE TIMEOUT at %d. Stopping.\n", current_pos);
        actuator_set_move_pins(act, 0);
        act->timeout = true;
    }

    // Stop if target reached
    if (abs(act->move_target - current_pos) <= POS_TOL) {
        actuator_set_move_pins(act, 0);
    }
}

void actuator_vref_init(void) {
    gpio_set_function(PIN_VREF, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PIN_VREF);
    pwm_set_wrap(slice_num, VREF_PWM_WRAP);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(PIN_VREF), VREF_MAX_DUTY);
    pwm_set_enabled(slice_num, true);
    printf("[ACTUATOR] VREF PWM Initialized on Pin %d\n", PIN_VREF);
}

void actuator_vref_set(double pid_output) {
    uint slice_num = pwm_gpio_to_slice_num(PIN_VREF);
    double abs_out = abs((int)pid_output);
    
    // Scale 0-500 PID range to PWM duty cycle
    if (abs_out > 500.0) abs_out = 500.0;
    
    uint32_t duty = VREF_MIN_DUTY + (uint32_t)((abs_out / 500.0) * (VREF_MAX_DUTY - VREF_MIN_DUTY));
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(PIN_VREF), duty);
}