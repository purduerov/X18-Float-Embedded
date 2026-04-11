#include "hardware/i2c.h"
#include "ms5837.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Custom Library Includes ---
#include "actuator.h"
#include "depth_pid.h"
#include "float_fsm.h"
#include "hw_config.h"
#include "hw_init.h"
#include "packets.h"
#include "radio_setup.h"
#include "reflash_target.h"
#include "storage.h"
#include "pid.h" // Generic PID for Actuator position

static float_fsm_t global_fsm;
static volatile bool float_radio_irq_flag = false;
static bool act_in_deadzone = false;

void onInterrupt(void) { float_radio_irq_flag = true; }

int main() {
  stdio_init_all();
  hw_wait_for_usb(FLOAT_ENABLE_USB_WAIT, 5000);

  printf("\n\n=== MATE Float Station Booting (Enhanced Control) ===\n");

  // --- Initialize Persistent Storage ---
  storage_init();

  // --- Initialize Hardware (I2C & Sensors) ---
  hw_init_i2c();

  MS5837_t depth_sensor;
  if (!hw_init_depth_sensor(&depth_sensor)) {
    printf("CRITICAL ERROR: MS5837 FAILED to initialize\n");
  }

  // --- Initialize Actuator ---
  actuator_vref_init();
  Actuator act;
  actuator_init(&act, PIN_POT, PIN_EXT, PIN_RET);
  
  // Inner Actuator PID
  PIDController act_pid;
  pid_init(&act_pid, ACT_KP, ACT_KI, ACT_KD, (ACT_LOOP_MS / 1000.0), -500.0, 500.0);

  // --- Initialize Depth PID ---
  DepthPID dpid;
  float_settings_t settings;
  storage_get_settings(&settings);

  depth_pid_init(&dpid, settings.kp, settings.ki, settings.kd, 0.1,
                 settings.act_min,
                 settings.act_max); // 100ms (10Hz) update rate
  depth_pid_set_target(&dpid, 1.0); // Set default target depth to 1.0m

  // --- Initialize Radio ---
  if (!radio_setup_init(onInterrupt)) {
    printf("CRITICAL ERROR: RADIO FAILED to initialize\n");
    while (true)
      sleep_ms(1000);
  }

  // --- Initialize State Machine ---
  float_fsm_init(&global_fsm, &depth_sensor);
  printf("Float System Ready. Target Depth: %.2f m\n", dpid.target_depth);
  printf("Serial Commands: 'z' (Zero Depth), 'a <pos>' (Actuator Position), 'p' (Profile), '?' (Sync)\n");

  uint32_t last_depth_pid_time = to_ms_since_boot(get_absolute_time());
  uint32_t last_act_loop_time = last_depth_pid_time;

  while (true) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // --- 1. Serial Command Handling (Dashboard Interface) ---
    static char input_line[64];
    static int input_pos = 0;
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
      if (c == '\n' || c == '\r') {
        if (input_pos > 0) {
          input_line[input_pos] = '\0';
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
                global_fsm.manual_move_pending = true;
                printf(">> [SERIAL] New Actuator Target: %d\n", global_fsm.actuator_target);
            }
          } else if (input_line[0] == 'p' || input_line[0] == 'P') {
            printf(">> [SERIAL] Starting Profile command via serial...\n");
            // FSM will handle profile transition if appropriate
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

    // --- 2. Outer Depth PID Loop (10Hz / 100ms) ---
    if (now - last_depth_pid_time >= 100) {
      double current_depth = 0.0f;
      if (ms5837_read(&depth_sensor)) {
        current_depth = ms5837_get_depth(&depth_sensor);
      }

      storage_get_settings(&settings);
      dpid.pid.kp = settings.kp;
      dpid.pid.ki = settings.ki;
      dpid.pid.kd = settings.kd;
      dpid.pos_min = settings.act_min;
      dpid.pos_max = settings.act_max;

      if (global_fsm.state == FLOAT_PROFILING) {
        int target_pos = 0;
        depth_pid_calculate_target_pos(&dpid, current_depth, &target_pos);
        global_fsm.actuator_target = target_pos;
      }

      last_depth_pid_time = now;
    }

    // --- 3. Inner Actuator Control Loop (50Hz / 20ms) ---
    if (now - last_act_loop_time >= ACT_LOOP_MS) {
      int current_pos = actuator_get_position(&act);
      int target_pos = global_fsm.actuator_target;

      // Safety Clamp
      storage_get_settings(&settings);
      if (target_pos < settings.act_min) target_pos = settings.act_min;
      if (target_pos > settings.act_max) target_pos = settings.act_max;

      double error = (double)target_pos - (double)current_pos;
      double control_signal = 0;

      // Hysteresis Logic
      if (!act_in_deadzone && abs((int)error) <= ACT_DEADZONE_ENTER) {
          act_in_deadzone = true;
      } else if (act_in_deadzone && abs((int)error) > ACT_DEADZONE_EXIT) {
          act_in_deadzone = false;
      }

      if (act_in_deadzone) {
          actuator_set_move_pins(&act, 0);
          pid_reset(&act_pid);
          actuator_vref_set(0);
      } else {
          pid_update(&act_pid, error, &control_signal);
          actuator_vref_set(control_signal);
          int direction = (control_signal > 0) ? 1 : -1;
          actuator_set_move_pins(&act, direction);
      }
      
      actuator_tick(&act);

      // Manual Move Status Update
      if (global_fsm.manual_move_pending) {
          if (act.stalled || act.timeout || act_in_deadzone) {
              global_fsm.manual_move_pending = false;
              if (act.stalled) printf(">> [MAIN] Manual move stalled!\n");
              if (act.timeout) printf(">> [MAIN] Manual move timeout!\n");
              if (act_in_deadzone) printf(">> [MAIN] Manual move reached target.\n");
          }
      }

      last_act_loop_time = now;
    }

    global_fsm.current_actuator_pos = actuator_get_position(&act);
    float_fsm_update(&global_fsm);

    if (float_radio_irq_flag) {
      float_radio_irq_flag = false;
      float_fsm_process_event(&global_fsm);
    }

    sleep_ms(1);
  }

  return 0;
}