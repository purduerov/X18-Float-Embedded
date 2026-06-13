#include "actuator.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    act->retry_count = 0;
    act->hard_locked = false;
    act->retry_timer = 0;

    // Initialize PID Controller
    pid_init(&act->pid, ACT_KP, ACT_KI, ACT_KD, (ACT_LOOP_MS / 1000.0), -ACT_PID_LIMIT, ACT_PID_LIMIT);
    act->in_deadzone = false;

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
    
    // 32 samples for stability (reduced from 256 for better loop timing)
    uint32_t sum_samples = 0;
    for(int i = 0; i < 32; i++) {
        sum_samples += adc_read();
    }
    int raw_pos = (int)(sum_samples / 32);

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

void actuator_set_target(Actuator *act, int target_pos) {
    if (act->move_target != target_pos) {
        act->move_target = target_pos;
        act->move_start_time = to_ms_since_boot(get_absolute_time());
        act->last_pos_time = act->move_start_time;
        act->last_pos = actuator_get_position(act);
        act->stalled = false;
        act->timeout = false;
        act->retry_count = 0;
        act->hard_locked = false;
        act->retry_timer = 0;
    }
}

void actuator_move_to(Actuator *act, int new_position) {
    actuator_set_target(act, new_position);
}

void actuator_tick(Actuator *act) {
    uint32_t t_now = to_ms_since_boot(get_absolute_time());
    int current_pos = actuator_get_position(act);
    double error = (double)act->move_target - (double)current_pos;
    double control_signal = 0;

    // 1. Hysteresis (Deadzone) Logic
    if (!act->in_deadzone && abs((int)error) <= ACT_DEADZONE_ENTER) {
        act->in_deadzone = true;
    } else if (act->in_deadzone && abs((int)error) > ACT_DEADZONE_EXIT) {
        act->in_deadzone = false;
    }

    // 2. Handle Auto-Retry Timer (If stalled but not hard-locked)
    if (act->stalled && !act->hard_locked) {
        if (t_now - act->retry_timer > ACT_RETRY_BACKOFF_MS) {
            printf(">> [ACTUATOR] Attempting auto-retry...\n");
            act->stalled = false;
            act->last_pos_time = t_now; 
            act->last_pos = current_pos;
            pid_reset(&act->pid);
        } else {
            actuator_set_move_pins(act, 0);
            actuator_vref_set(0);
            return; 
        }
    }

    // 3. Stop if Hard Locked, Stalled (waiting for timer), or in Deadzone
    if (act->hard_locked || act->stalled || act->in_deadzone) {
        actuator_set_move_pins(act, 0);
        actuator_vref_set(0);
        pid_reset(&act->pid);
        
        // Reset move timer if in deadzone so next move starts fresh
        if (act->in_deadzone) {
            act->move_start_time = 0;
        }
        return;
    }

    // 4. Movement Monitoring (Stall & Timeout)
    if (act->move_start_time == 0) {
        act->move_start_time = t_now;
        act->last_pos_time = t_now;
        act->last_pos = current_pos;
    }

    if (abs(current_pos - act->last_pos) >= ACT_STALL_THRESHOLD) {
        act->last_pos = current_pos;
        act->last_pos_time = t_now;
    }

    if (t_now - act->last_pos_time > ACT_STALL_MS) {
        actuator_set_move_pins(act, 0);
        actuator_vref_set(0);
        pid_reset(&act->pid);
        if (act->retry_count < ACT_MAX_RETRIES) {
            act->stalled = true;
            act->retry_count++;
            act->retry_timer = t_now;
            printf("!! [ACTUATOR] STALL DETECTED at %d. Retrying in %d ms...\n", current_pos, ACT_RETRY_BACKOFF_MS);
        } else {
            act->hard_locked = true;
            printf("!! [ACTUATOR] HARD LOCK: Multiple stalls at %d.\n", current_pos);
        }
        return;
    } else if (t_now - act->move_start_time > ACT_MOVE_TIMEOUT_MS) {
        printf("!! [ACTUATOR] MOVE TIMEOUT at %d.\n", current_pos);
        actuator_set_move_pins(act, 0);
        actuator_vref_set(0);
        pid_reset(&act->pid);
        act->timeout = true;
        return;
    }

    // 5. Active Control (PID)
    pid_update(&act->pid, error, &control_signal);
    actuator_vref_set(control_signal);
    int direction = (control_signal > 0) ? 1 : -1;
    actuator_set_move_pins(act, direction);
}

void actuator_vref_init(void) {
    gpio_set_function(PIN_VREF, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PIN_VREF);
    pwm_set_wrap(slice_num, ACT_VREF_PWM_WRAP);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(PIN_VREF), ACT_VREF_MAX_DUTY);
    pwm_set_enabled(slice_num, true);
    printf("[ACTUATOR] VREF PWM Initialized on Pin %d\n", PIN_VREF);
}

void actuator_vref_set(double pid_output) {
    uint slice_num = pwm_gpio_to_slice_num(PIN_VREF);
    double abs_out = fabs(pid_output);
    
    // Scale PID range to PWM duty cycle
    if (abs_out > ACT_PID_LIMIT) abs_out = ACT_PID_LIMIT;
    
    uint32_t duty = ACT_VREF_MIN_DUTY + (uint32_t)((abs_out / ACT_PID_LIMIT) * (ACT_VREF_MAX_DUTY - ACT_VREF_MIN_DUTY));
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(PIN_VREF), duty);
}
