#include "actuator.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pid.h"
#include <stdio.h>
#include <stdlib.h>

// Hardware Setup
#define PIN_POT 26
#define PIN_EXT 12
#define PIN_RET 13
#define PIN_VREF 27

// PID Tuning (Slightly increased D to act as a brake)
#define KP 0.8
#define KI 0.05
#define KD 0.15
#define LOOP_DELAY_MS 20

// Hysteresis Logic
#define DEADZONE_ENTER 20 // Stop moving when within 20 units
#define DEADZONE_EXIT 50  // Do not resume unless pushed 50 units away
bool in_deadzone = false;

// Dynamic VREF Settings (16-bit PWM on Pico)
#define PWM_WRAP 65535
#define VREF_MIN_DUTY 19859 // Roughly 1.0V (prevents motor stalling too early)
#define VREF_MAX_DUTY 65535 // 3.3V (Maximum torque)

// Moving Average Filter (Smooths out ADC noise)
#define FILTER_SIZE 5
int pos_history[FILTER_SIZE] = {0};
int filter_idx = 0;

// Rotation Settings
#define DWELL_TIME_MS 2000
int targets[] = {100};
int current_target_idx = 0;
uint32_t reached_time_ms = 0;
bool is_waiting = false;

// Initialize Pin 27 for Hardware PWM
void init_vref_pwm() {
  gpio_set_function(PIN_VREF, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(PIN_VREF);
  pwm_set_wrap(slice_num, PWM_WRAP);
  pwm_set_chan_level(slice_num, pwm_gpio_to_channel(PIN_VREF), VREF_MAX_DUTY);
  pwm_set_enabled(slice_num, true);
}

// Map PID output to a safe VREF voltage
void set_vref_voltage(double pid_output) {
  uint slice_num = pwm_gpio_to_slice_num(PIN_VREF);
  double abs_out = abs((int)pid_output);

  // Clamp the PID mapping range
  if (abs_out > 500)
    abs_out = 500;

  // Map absolute PID output to PWM duty cycle
  uint32_t duty = VREF_MIN_DUTY + (uint32_t)((abs_out / 500.0) *
                                             (VREF_MAX_DUTY - VREF_MIN_DUTY));
  pwm_set_chan_level(slice_num, pwm_gpio_to_channel(PIN_VREF), duty);
}

// Get noise-filtered ADC position
int get_filtered_pos(Actuator *act) {
  pos_history[filter_idx] = actuator_get_position(act);
  filter_idx = (filter_idx + 1) % FILTER_SIZE;
  long sum = 0;
  for (int i = 0; i < FILTER_SIZE; i++) {
    sum += pos_history[i];
  }
  return sum / FILTER_SIZE;
}

int main() {
  stdio_init_all();

  while (!stdio_usb_connected()) {
    sleep_ms(100);
  }
  printf("Serial link established. System starting...\n");

  // Initialize the VREF PWM instead of a static GPIO high
  init_vref_pwm();

  Actuator act;
  actuator_init(&act, PIN_POT, PIN_EXT, PIN_RET);

  PIDController pid;
  // Expanded limits to map directly to our 0-500 scale for VREF
  pid_init(&pid, KP, KI, KD, (LOOP_DELAY_MS / 1000.0), -500.0, 500.0);

  // Pre-fill the moving average filter
  for (int i = 0; i < FILTER_SIZE; i++)
    get_filtered_pos(&act);

  while (1) {
    int current_pos = get_filtered_pos(&act);
    int target = targets[current_target_idx];
    double error = (double)target - (double)current_pos;
    double control_signal = 0;

    // 1. Dual-Threshold Hysteresis Evaluation
    if (!in_deadzone && abs((int)error) <= DEADZONE_ENTER) {
      in_deadzone = true;
    } else if (in_deadzone && abs((int)error) > DEADZONE_EXIT) {
      in_deadzone = false;
    }

    // 2. Control Logic
    if (in_deadzone) {
      actuator_set_move_pins(&act, 0);
      pid_reset(&pid);
      set_vref_voltage(0); // Drop current limit to minimum holding torque

      if (!is_waiting) {
        reached_time_ms = to_ms_since_boot(get_absolute_time());
        is_waiting = true;
        printf("Target %d reached! Waiting %dms...\n", target, DWELL_TIME_MS);
      }

      if (to_ms_since_boot(get_absolute_time()) - reached_time_ms >=
          DWELL_TIME_MS) {
        current_target_idx =
            (current_target_idx + 1) % (sizeof(targets) / sizeof(targets[0]));
        is_waiting = false;
        printf("Switching to target: %d\n", targets[current_target_idx]);
      }
    } else {
      is_waiting = false;
      pid_update(&pid, error, &control_signal);

      // 3. Dynamic Braking via VREF
      set_vref_voltage(control_signal);

      int direction = (control_signal > 0) ? 1 : -1;
      actuator_set_move_pins(&act, direction);
    }

    // Telemetry
    printf("Target:%d, Current:%d, Error:%.1f, Output:%.2f, InZone:%d\n",
           target, current_pos, error, control_signal, in_deadzone);

    sleep_ms(LOOP_DELAY_MS);
  }
  return 0;
}