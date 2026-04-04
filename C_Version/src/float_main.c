#include "hardware/i2c.h"
#include "ms5837.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- Custom Library Includes ---
#include "actuator.h"
#include "depth_pid.h"
#include "float_fsm.h"
#include "packets.h"
#include "radio_setup.h"
#include "storage.h"
#include "reflash_target.h"
#include "hw_config.h"

static float_fsm_t global_fsm;
static volatile bool float_radio_irq_flag = false;

void onInterrupt(void) { float_radio_irq_flag = true; }

int main() {
  stdio_init_all();

  uint32_t waitTime = 0;
  while (!stdio_usb_connected() && waitTime < 5000) {
    sleep_ms(100);
    waitTime += 100;
  }

  printf("\n\n=== MATE Float Station Booting (PID Enabled) ===\n");

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
  Actuator act;
  actuator_init(&act, PIN_POT, PIN_EXT, PIN_RET);
  gpio_init(PIN_VREF);
  gpio_set_dir(PIN_VREF, GPIO_OUT);
  gpio_put(PIN_VREF, 1); // Enable full power to motor driver

  // --- Initialize Depth PID ---
  DepthPID dpid;
  float_settings_t settings;
  storage_get_settings(&settings);
  // 100ms (10Hz) update rate
  depth_pid_init(&dpid, settings.kp, settings.ki, settings.kd, 0.1, settings.act_min, settings.act_max);
  depth_pid_set_target(&dpid, 1.0); // Set default target depth to 1.0m

  // --- Initialize Radio ---
  if (!radio_setup_init(onInterrupt)) {
    printf("Radio init failed! Halting.\n");
    while (true)
      sleep_ms(1000);
  }

  // --- Initialize State Machine ---
  float_fsm_init(&global_fsm, &depth_sensor);

  printf("Float System Ready. Target Depth: %.2f m\n", dpid.target_depth);

  uint32_t last_pid_time = to_ms_since_boot(get_absolute_time());

  while (true) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Run PID loop at 10Hz
    if (now - last_pid_time >= 100) {
      // 1. Refresh depth sensor
      ms5837_read(&depth_sensor);
      double current_depth = ms5837_get_depth(&depth_sensor);

      // 2. Refresh PID constants (in case they were updated via radio)
      storage_get_settings(&settings);
      dpid.pid.kp = settings.kp;
      dpid.pid.ki = settings.ki;
      dpid.pid.kd = settings.kd;
      dpid.pos_min = settings.act_min;
      dpid.pos_max = settings.act_max;
      dpid.pid.output_min = (double)settings.act_min;
      dpid.pid.output_max = (double)settings.act_max;

      // 3. Calculate target actuator position
      int target_pos = 0;
      if (global_fsm.state == FLOAT_PROFILING) {
          depth_pid_calculate_target_pos(&dpid, current_depth, &target_pos);
      } else {
          target_pos = global_fsm.actuator_target;
      }

      // Safety Clamp: Ensure target is within configured bounds
      if (target_pos < settings.act_min) target_pos = settings.act_min;
      if (target_pos > settings.act_max) target_pos = settings.act_max;

      // 4. Command Actuator & Update monitoring (stop if reached)
      int current_pos = actuator_get_position(&act);
      
      // If a manual move command is pending, enter the blocking loop once
      if (global_fsm.state == FLOAT_IDLE && global_fsm.manual_move_pending) {
          global_fsm.manual_move_pending = false; // Reset immediately to prevent re-triggering
          printf(">> Actuator Moving to %d...\n", target_pos);
          
          uint32_t start_time = to_ms_since_boot(get_absolute_time());
          uint32_t last_move_time = start_time;
          int last_pos = current_pos;

          while (abs(actuator_get_position(&act) - target_pos) > POS_TOL) {
              actuator_move_to(&act, target_pos);
              sleep_ms(20);
              
              int p = actuator_get_position(&act);
              uint32_t t_now = to_ms_since_boot(get_absolute_time());
              
              if (abs(p - last_pos) > 2) {
                  last_pos = p;
                  last_move_time = t_now;
              }
              
              if (t_now - last_move_time > 500) {
                  printf(">> Actuator Stalled! Stopping to prevent damage.\n");
                  break;
              }
              
              if (t_now - start_time > 5000) {
                  printf(">> Actuator Timeout! Stopping.\n");
                  break;
              }
          }
          
          actuator_set_move_pins(&act, 0);
          printf(">> Actuator Stopped at %d.\n", actuator_get_position(&act));
      } else {
          // Normal non-blocking PID operation during profiling or idle maintenance
          if (abs(current_pos - target_pos) <= POS_TOL) {
            actuator_set_move_pins(&act, 0);
          } else {
            actuator_move_to(&act, target_pos);
          }
      }

      last_pid_time = now;
    }

    global_fsm.current_actuator_pos = actuator_get_position(&act);
    float_fsm_update(&global_fsm);

    // Process Radio Events outside of ISR
    if (float_radio_irq_flag) {
        float_radio_irq_flag = false;
        float_fsm_process_event(&global_fsm);
    }

    sleep_ms(1);
  }

  return 0;
}