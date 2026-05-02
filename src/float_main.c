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
#include "sw_config.h"
#include "hw_init.h"
#include "packets.h"
#include "radio_setup.h"
#include "reflash_target.h"
#include "storage.h"
#include "pid.h" 
#include "console.h"
#include "neopixel.h"

static float_fsm_t global_fsm;
static volatile bool radio_event_flag = false;
static MS5837_t depth_sensor;
static DepthPID dpid;

void onInterrupt(void) { radio_event_flag = true; }

// --- Console Command Handlers ---

static void handle_zero(const char *params) {
    float_settings_t settings;
    storage_get_settings(&settings);
    ms5837_read(&depth_sensor);
    settings.depth_offset = ms5837_get_depth(&depth_sensor);
    printf(">> [CONSOLE] Depth Zeroed at: %.3f m. Saving to Flash...\n", settings.depth_offset);
    storage_set_settings(&settings);
    storage_save();
}

static void handle_sync(const char *params) {
    float_settings_t settings;
    storage_get_settings(&settings);
    // ADC= is used by dashboard for live actuator position. 
    printf("[SYNC] P=%.2f I=%.2f D=%.2f Deep=%.2f Shallow=%.2f N=%u Co#=%u Time=%u Off=%.3f ADC=%d ActMin=%d ActMax=%d Neutral=%d LiveDepth=%.3f Tol=%.2f\n",
           settings.kp, settings.ki, settings.kd, 
           settings.deep_target_m, settings.shallow_target_m, settings.num_profiles,
           settings.company_number, settings.profile_duration_s, 
           settings.depth_offset, global_fsm.current_actuator_pos,
           settings.act_min, settings.act_max, settings.neutral_buoyancy_adc,
           global_fsm.current_depth, settings.arrival_band_m);
}

static void handle_actuator(const char *params) {
    int parsed_pos;
    if (sscanf(params, "%d", &parsed_pos) == 1) {
        global_fsm.actuator_target = parsed_pos;
        global_fsm.manual_move_pending = true;
        printf(">> [CONSOLE] New Actuator Target: %d\n", global_fsm.actuator_target);
        handle_sync(NULL);
    }
}

static void handle_profile(const char *params) {
    printf(">> [CONSOLE] Starting Profile command via serial...\n");
}

static const console_command_t cmd_table[] = {
    {'z', handle_zero, "Zero Depth"},
    {'a', handle_actuator, "Set Actuator Position"},
    {'p', handle_profile, "Start Profile"},
    {'?', handle_sync, "Sync Settings"}};

int main() {
  // system_init handles everything including stdio_init_all
  if (!system_init(onInterrupt, &depth_sensor)) {
      while (true) sleep_ms(1000);
  }

  // --- Initialize Status LED ---
  neopixel_init(PIN_NEOPIXEL);

  // --- Initialize High Level Objects ---
  Actuator act;
  actuator_init(&act, PIN_POT, PIN_EXT, PIN_RET);

  float_settings_t settings;
  storage_get_settings(&settings);

  // We now use the PID to calculate absolute positions.
  // The min/max limits here represent the allowable PID OFFSET from neutral.
  // We use +/- 2048 to allow full range coverage around any neutral point.
  depth_pid_init(&dpid, settings.kp, settings.ki, settings.kd, 0.1, -2048, 2048);
  depth_pid_set_target(&dpid, settings.deep_target_m);

  console_init(cmd_table, sizeof(cmd_table) / sizeof(console_command_t));
  float_fsm_init(&global_fsm, &depth_sensor);

  printf("Float System Ready.\n");

  uint32_t last_depth_pid_time = to_ms_since_boot(get_absolute_time());
  uint32_t last_act_loop_time = last_depth_pid_time;
  FloatState_t prev_state = FLOAT_IDLE;
  uint32_t consecutive_sensor_failures = 0;

  while (true) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    console_update();

    storage_get_settings(&settings);

    // --- 2. Outer Depth PID Loop (10Hz) ---
    if (now - last_depth_pid_time >= DEPTH_PID_LOOP_MS) {
      double current_depth = 10000.0f; // Default to error indicator
      
      // --- SENSOR READ & RECOVERY ---
      if (ms5837_read(&depth_sensor)) {
        consecutive_sensor_failures = 0;
        float depth = ms5837_get_depth(&depth_sensor) - settings.depth_offset;
        current_depth = (double)depth;
        global_fsm.current_depth = depth; // Sync for FSM use
      } else {
        consecutive_sensor_failures++;
        
        // TIER 1 RECOVERY: Simple Sensor Reset
        if (consecutive_sensor_failures == SENSOR_RESET_STRIKES) {
          printf("!! [SENSOR] Reading Failed. Attempting Sensor Reset...\n");
          ms5837_begin(&depth_sensor, I2C_PORT, MS5837_UNRECOGNISED);
        }
        
        // TIER 2 RECOVERY: Full I2C Bus Reset
        static uint32_t last_bus_reset_time = 0;
        if (consecutive_sensor_failures >= I2C_BUS_RESET_STRIKES && (now - last_bus_reset_time > I2C_BUS_RESET_THROTTLE_MS)) {
          printf("!! [SENSOR] Persistent Failure. Performing FULL I2C RESET...\n");
          hw_deinit_i2c();
          sleep_ms(10);
          hw_init_i2c();
          ms5837_begin(&depth_sensor, I2C_PORT, MS5837_UNRECOGNISED);
          last_bus_reset_time = now;
        }

        // FAIL-SAFE: Mission Abort
        if (consecutive_sensor_failures >= SENSOR_ABORT_STRIKES && global_fsm.state == FLOAT_PROFILING) {
          printf(">> [CRITICAL] DEPTH SENSOR LOST. ABORTING MISSION!\n");
          global_fsm.state = FLOAT_PROFILE_DONE;
          global_fsm.actuator_target = settings.act_max; // Surface immediately
          consecutive_sensor_failures = 0; // Don't spam abort
        }
      }

      dpid.pid.kp = settings.kp;
      dpid.pid.ki = settings.ki;
      dpid.pid.kd = settings.kd;

      if (global_fsm.state == FLOAT_PROFILING) {
        float target_m = (global_fsm.mission_stage == STAGE_DEEP) ? settings.deep_target_m : settings.shallow_target_m;
        depth_pid_set_target(&dpid, target_m);
        
        // Reset PID and sync target on first entry to profiling
        if (prev_state != FLOAT_PROFILING) {
            printf(">> PID: Entering PROFILING mode. Resetting Integral and syncing target.\n");
            depth_pid_reset(&dpid);
            global_fsm.actuator_target = global_fsm.current_actuator_pos;
        }

        int current_target = (int)global_fsm.actuator_target;
        depth_pid_calculate_target_pos(&dpid, current_depth, settings.neutral_buoyancy_adc, &current_target);
        global_fsm.actuator_target = (uint16_t)current_target;
      }
      
      prev_state = global_fsm.state;
      last_depth_pid_time = now;
    }

    // --- 3. Inner Actuator Control Loop ---
    if (now - last_act_loop_time >= ACT_LOOP_MS) {
      int target_pos = global_fsm.actuator_target;
      if (target_pos < settings.act_min) target_pos = settings.act_min;
      if (target_pos > settings.act_max) target_pos = settings.act_max;

      actuator_set_target(&act, target_pos);
      actuator_tick(&act);
      
      if (global_fsm.manual_move_pending) {
          if (act.stalled || act.timeout || act.in_deadzone || act.hard_locked) {
              global_fsm.manual_move_pending = false;
          }
      }
      last_act_loop_time = now;
    }

    global_fsm.current_actuator_pos = actuator_get_position(&act);
    float_fsm_update(&global_fsm);

    if (radio_event_flag) {
      radio_event_flag = false;
      float_fsm_process_event(&global_fsm);
    }
    sleep_ms(1);
  }
  return 0;
}
