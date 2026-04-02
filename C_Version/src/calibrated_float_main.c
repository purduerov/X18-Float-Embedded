#include "hardware/i2c.h"
#include "ms5837.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- Custom Library Includes ---
#include "float_fsm.h"
#include "packets.h"
#include "radio_setup.h"
#include "storage.h"
#include "actuator.h"
#include "pid.h"

// --- I2C / Sensor Setup ---
#define I2C_PORT i2c1
#define PIN_SDA 2
#define PIN_SCL 3

// --- Actuator Hardware Setup ---
#define PIN_POT 26
#define PIN_EXT 12
#define PIN_RET 13
#define PIN_VREF 27

// --- Actuator PID Tuning ---
#define ACT_KP 0.8
#define ACT_KI 0.05
#define ACT_KD 0.15
#define LOOP_DELAY_MS 20

// Hysteresis Logic
#define DEADZONE_ENTER 20
#define DEADZONE_EXIT 50
static bool act_in_deadzone = false;

// Dynamic VREF Settings
#define PWM_WRAP 65535
#define VREF_MIN_DUTY 19859
#define VREF_MAX_DUTY 65535

// Moving Average Filter
#define FILTER_SIZE 5
static int pos_history[FILTER_SIZE] = {0};
static int filter_idx = 0;

static float_fsm_t global_fsm;

void onInterrupt(void) { float_fsm_on_interrupt(&global_fsm); }

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
  if (abs_out > 500) abs_out = 500;
  uint32_t duty = VREF_MIN_DUTY + (uint32_t)((abs_out / 500.0) * (VREF_MAX_DUTY - VREF_MIN_DUTY));
  pwm_set_chan_level(slice_num, pwm_gpio_to_channel(PIN_VREF), duty);
}

// Get noise-filtered ADC position
int get_filtered_pos(Actuator *act) {
  pos_history[filter_idx] = actuator_get_position(act);
  filter_idx = (filter_idx + 1) % FILTER_SIZE;
  long sum = 0;
  for (int i = 0; i < FILTER_SIZE; i++) sum += pos_history[i];
  return (int)(sum / FILTER_SIZE);
}

int main() {
  stdio_init_all();

  // Wait for USB connection for a bit
  uint32_t waitTime = 0;
  while (!stdio_usb_connected() && waitTime < 3000) {
    sleep_ms(100);
    waitTime += 100;
  }

  printf("\n\n=== X18 Float Station Booting (Calibrated Main + Actuator) ===\n");

  // --- Initialize Persistent Storage ---
  storage_init();

  // --- Initialize I2C and MS5837 ---
  i2c_init(I2C_PORT, 400 * 1000);
  gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
  gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(PIN_SDA);
  gpio_pull_up(PIN_SCL);

  MS5837_t depth_sensor;
  ms5837_init_struct(&depth_sensor);
  if (!ms5837_begin(&depth_sensor, I2C_PORT, MS5837_02BA)) {
    printf("CRITICAL ERROR: MS5837 FAILED to initialize\n");
  }

  // --- Initialize Actuator ---
  init_vref_pwm();
  Actuator act;
  actuator_init(&act, PIN_POT, PIN_EXT, PIN_RET);
  
  PIDController act_pid;
  pid_init(&act_pid, ACT_KP, ACT_KI, ACT_KD, (LOOP_DELAY_MS / 1000.0), -500.0, 500.0);

  for (int i = 0; i < FILTER_SIZE; i++) get_filtered_pos(&act);

  // --- Initialize Radio ---
  if (!radio_setup_init(onInterrupt)) {
    printf("Radio init failed! Halting.\n");
    while (true) sleep_ms(1000);
  }

  // --- Initialize State Machine ---
  float_fsm_init(&global_fsm, &depth_sensor);

  printf("Float System Ready.\n");
  printf("Serial Commands: 'z' (Zero Depth), 'a <pos>' (Actuator Position), 'p' (Profile), '?' (Sync)\n");

  uint32_t last_act_update = to_ms_since_boot(get_absolute_time());

  while (true) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // --- Serial Command Handling ---
    static char input_line[64];
    static int input_pos = 0;

    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
      if (c == '\n' || c == '\r') {
        if (input_pos > 0) {
          input_line[input_pos] = '\0';
          
          float_settings_t settings;
          storage_get_settings(&settings);

          if (input_line[0] == 'z' || input_line[0] == 'Z') {
            ms5837_read(&depth_sensor);
            settings.depth_offset = ms5837_get_depth(&depth_sensor);
            printf(">> [SERIAL] Depth Zeroed at: %.3f m. Saving to Flash...\n", settings.depth_offset);
            storage_set_settings(&settings);
            storage_save();
          } else if (input_line[0] == 'a' || input_line[0] == 'A') {
            int parsed_pos;
            if (sscanf(input_line + 1, "%d", &parsed_pos) == 1) {
                global_fsm.actuator_target = parsed_pos;
                printf(">> [SERIAL] New Actuator Target: %d\n", global_fsm.actuator_target);
            }
          } else if (input_line[0] == 'p' || input_line[0] == 'P') {
            printf(">> [SERIAL] Starting Profile command via serial...\n");
          } else if (input_line[0] == '?') {
             printf("[SYNC] P=%.2f I=%.2f D=%.2f Co#=%u Time=%u Off=%.3f Act=%d\n",
                   settings.kp, settings.ki, settings.kd, 
                   settings.company_number, settings.profile_duration_s, 
                   settings.depth_offset, global_fsm.actuator_target);
          }
          input_pos = 0;
        }
      } else if (c >= 32 && c <= 126) {
        if (input_pos < sizeof(input_line) - 1) {
          input_line[input_pos++] = (char)c;
        }
      }
    }

    // --- Actuator Control Loop (20ms) ---
    if (now - last_act_update >= LOOP_DELAY_MS) {
        int current_pos = get_filtered_pos(&act);
        double error = (double)global_fsm.actuator_target - (double)current_pos;
        double control_signal = 0;

        if (!act_in_deadzone && abs((int)error) <= DEADZONE_ENTER) {
            act_in_deadzone = true;
        } else if (act_in_deadzone && abs((int)error) > DEADZONE_EXIT) {
            act_in_deadzone = false;
        }

        if (act_in_deadzone) {
            actuator_set_move_pins(&act, 0);
            pid_reset(&act_pid);
            set_vref_voltage(0);
        } else {
            pid_update(&act_pid, error, &control_signal);
            set_vref_voltage(control_signal);
            int direction = (control_signal > 0) ? 1 : -1;
            actuator_set_move_pins(&act, direction);
        }
        last_act_update = now;
    }

    float_fsm_update(&global_fsm);
    sleep_ms(1);
  }

  return 0;
}