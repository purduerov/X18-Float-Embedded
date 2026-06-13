#include "hardware/i2c.h"
#include "ms5837.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

void onInterrupt(void) { radio_event_flag = true; }

// --- Console Command Handlers ---

static void handle_zero(const char *params) {
    float_settings_t settings;
    storage_get_settings(&settings);
    if (ms5837_read(&depth_sensor)) {
        settings.depth_offset = ms5837_get_depth(&depth_sensor);
        printf(">> [CONSOLE] Depth Zeroed at: %.3f m. Saving to Flash...\n", settings.depth_offset);
        storage_set_settings(&settings);
        storage_save();
    } else {
        printf("!! [CONSOLE] Depth sensor read FAILED. Zero aborted.\n");
    }
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
    if (global_fsm.state == FLOAT_IDLE) {
        global_fsm.state = FLOAT_PRE_DIVE;
    } else {
        printf(">> [CONSOLE] Ignoring: FSM must be in IDLE to start a profile.\n");
    }
}

#ifdef HIL_MODE
// HIL mode global state — shared between handle_hil() and the main loop
volatile float hil_depth = 0.0f;
volatile float hil_pressure = 101.325f;
#endif

static void handle_hil(const char *params) {
#ifdef HIL_MODE
    float depth;
    if (sscanf(params, "%f", &depth) == 1) {
        hil_depth = depth;
        hil_pressure = (depth * 1000.0f * 9.80665f + 101325.0f) / 1000.0f;
    }
#else
    (void)params;
#endif
}

static void handle_team(const char *params) {
    int val;
    if (sscanf(params, "%d", &val) == 1) {
        float_settings_t settings;
        storage_get_settings(&settings);
        settings.company_number = (uint16_t)val;
        storage_set_settings(&settings);
        storage_save();
        printf(">> [CONSOLE] Company ID set to %u\n", val);
        handle_sync(NULL);
    }
}

static void handle_duration(const char *params) {
    int val;
    if (sscanf(params, "%d", &val) == 1) {
        float_settings_t settings;
        storage_get_settings(&settings);
        settings.profile_duration_s = (uint16_t)val;
        storage_set_settings(&settings);
        storage_save();
        printf(">> [CONSOLE] Duration set to %u s\n", val);
        handle_sync(NULL);
    }
}

static void handle_deep(const char *params) {
    float val;
    if (sscanf(params, "%f", &val) == 1) {
        float_settings_t settings;
        storage_get_settings(&settings);
        settings.deep_target_m = val;
        storage_set_settings(&settings);
        storage_save();
        printf(">> [CONSOLE] Deep Target set to %.2f m\n", val);
        handle_sync(NULL);
    }
}

static void handle_shallow(const char *params) {
    float val;
    if (sscanf(params, "%f", &val) == 1) {
        float_settings_t settings;
        storage_get_settings(&settings);
        settings.shallow_target_m = val;
        storage_set_settings(&settings);
        storage_save();
        printf(">> [CONSOLE] Shallow Target set to %.2f m\n", val);
        handle_sync(NULL);
    }
}

static void handle_profiles(const char *params) {
    int val;
    if (sscanf(params, "%d", &val) == 1) {
        float_settings_t settings;
        storage_get_settings(&settings);
        settings.num_profiles = (uint16_t)val;
        storage_set_settings(&settings);
        storage_save();
        printf(">> [CONSOLE] Profile Count set to %u\n", val);
        handle_sync(NULL);
    }
}

static void handle_tolerance(const char *params) {
    float val;
    if (sscanf(params, "%f", &val) == 1) {
        float_settings_t settings;
        storage_get_settings(&settings);
        settings.arrival_band_m = val;
        storage_set_settings(&settings);
        storage_save();
        printf(">> [CONSOLE] Tolerance set to %.2f m\n", val);
        handle_sync(NULL);
    }
}

static void handle_pid_console(const char *params) {
    float p, i, d;
    if (sscanf(params, "%f %f %f", &p, &i, &d) == 3) {
        float_settings_t settings;
        storage_get_settings(&settings);
        settings.kp = p;
        settings.ki = i;
        settings.kd = d;
        storage_set_settings(&settings);
        storage_save();
        printf(">> [CONSOLE] PID set to P=%.2f I=%.2f D=%.2f\n", p, i, d);
        handle_sync(NULL);
    }
}

static void handle_bounds_console(const char *params) {
    unsigned int min, max;
    if (sscanf(params, "%u %u", &min, &max) == 2) {
        float_settings_t settings;
        storage_get_settings(&settings);
        settings.act_min = (uint16_t)min;
        settings.act_max = (uint16_t)max;
        storage_set_settings(&settings);
        storage_save();
        printf(">> [CONSOLE] Bounds set to %u - %u\n", min, max);
        handle_sync(NULL);
    }
}

static void handle_neutral_console(const char *params) {
    int val;
    if (sscanf(params, "%d", &val) == 1) {
        float_settings_t settings;
        storage_get_settings(&settings);
        settings.neutral_buoyancy_adc = (uint16_t)val;
        storage_set_settings(&settings);
        storage_save();
        printf(">> [CONSOLE] Neutral ADC set to %u\n", val);
        handle_sync(NULL);
    }
}

static void handle_reset_console(const char *params) {
    printf(">> [CONSOLE] Resetting FSM to IDLE...\n");
    global_fsm.state = FLOAT_IDLE;
    global_fsm.current_profile = 0;
}

static const console_command_t cmd_table[] = {
    {'z', handle_zero, "Zero Depth"},
    {'a', handle_actuator, "Set Actuator Position"},
    {'p', handle_profile, "Start Profile"},
    {'c', handle_team, "Set Team ID"},
    {'t', handle_duration, "Set Duration"},
    {'d', handle_deep, "Set Deep Target"},
    {'u', handle_shallow, "Set Shallow Target"},
    {'m', handle_profiles, "Set Num Profiles"},
    {'v', handle_tolerance, "Set Tolerance"},
    {'s', handle_pid_console, "Set PID Gains"},
    {'b', handle_bounds_console, "Set Actuator Bounds"},
    {'n', handle_neutral_console, "Set Neutral ADC"},
    {'r', handle_reset_console, "Reset FSM"},
    {'?', handle_sync, "Sync Settings"},
#ifdef HIL_MODE
    {'h', handle_hil, "HIL Update"},
#endif
};

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

  console_init(cmd_table, sizeof(cmd_table) / sizeof(console_command_t));
  float_fsm_init(&global_fsm, &depth_sensor);

  printf("Float System Ready. Build: %s %s\n", __DATE__, __TIME__);

  uint32_t last_depth_pid_time = to_ms_since_boot(get_absolute_time());
  uint32_t last_act_loop_time = last_depth_pid_time;
  FloatState_t prev_state = FLOAT_IDLE;
  uint32_t consecutive_sensor_failures = 0;

  while (true) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    console_update();

    storage_get_settings(&settings);

    // --- 2. Outer Depth Loop (10Hz) ---
    if (now - last_depth_pid_time >= DEPTH_PID_LOOP_MS) {
      double current_depth = 10000.0f; // Default to error indicator
      
      // --- SENSOR READ & RECOVERY ---
      if (ms5837_read(&depth_sensor)) {
        consecutive_sensor_failures = 0;
#ifdef HIL_MODE
        float depth = ms5837_get_depth(&depth_sensor);
#else
        float depth = ms5837_get_depth(&depth_sensor) - settings.depth_offset;
#endif
        current_depth = (double)depth;
        
        // Update velocity (EMA filtered)
        if (prev_state == FLOAT_PROFILING) {
          float dt = (now - last_depth_pid_time) / 1000.0f;
          if (dt <= 0.0f) dt = 0.1f; // Prevent div by zero
          float raw_velocity = (depth - global_fsm.last_depth) / dt;
          global_fsm.filtered_velocity = (VELOCITY_EMA_ALPHA * raw_velocity) + ((1.0f - VELOCITY_EMA_ALPHA) * global_fsm.filtered_velocity);
        } else {
          global_fsm.filtered_velocity = 0.0f;
        }
        global_fsm.last_depth = depth;
        global_fsm.current_depth = depth; // Sync for FSM use
      } else {
        consecutive_sensor_failures++;
        
        // TIER 1 RECOVERY: Simple Sensor Reset
        if (consecutive_sensor_failures >= SENSOR_RESET_STRIKES) {
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

      if (global_fsm.state == FLOAT_PROFILING) {
        float nominal_target = 0.0f;
        float effective_target = 0.0f;
        
        if (global_fsm.mission_stage == STAGE_DEEP) {
          nominal_target = settings.deep_target_m;
          effective_target = settings.deep_target_m;
        } else if (global_fsm.mission_stage == STAGE_SHALLOW) {
          nominal_target = settings.shallow_target_m;
          effective_target = SHALLOW_BIASED_TARGET_M; // BIASED TARGET to avoid breaking surface
        } else if (global_fsm.mission_stage == STAGE_EXITING) {
          nominal_target = -0.5f;
          effective_target = -0.5f;
        }

        // Initialize active baseline on first entry
        if (prev_state != FLOAT_PROFILING) {
            printf(">> Control: Entering PROFILING mode. Base Neutral ADC: %d\n", settings.neutral_buoyancy_adc);
            global_fsm.active_neutral_adc = settings.neutral_buoyancy_adc;
            global_fsm.ctrl_state = 0; // CTRL_TRANSIT
            global_fsm.actuator_target = settings.neutral_buoyancy_adc;
        }

        float depth_error = current_depth - effective_target;
        
        // --- Hard Recovery Check ---
        // If we drift too far AND are moving away, reset back to Transit
        bool hard_drift = false;
        if (global_fsm.ctrl_state == CTRL_HOVER) {
            if (fabs(current_depth - nominal_target) > HOVER_RECOVERY_M) {
                // Only bail out if we are moving in the wrong direction
                if ((current_depth > nominal_target && global_fsm.filtered_velocity > 0.05f) ||
                    (current_depth < nominal_target && global_fsm.filtered_velocity < -0.05f)) {
                    hard_drift = true;
                }
            }
        } else if (global_fsm.ctrl_state == CTRL_BRAKING) {
            // Give braking more leeway; only abort if wildly off course
            if (global_fsm.mission_stage == STAGE_DEEP) {
                if (current_depth > nominal_target + (HOVER_RECOVERY_M * 1.5f)) hard_drift = true;
            } else if (global_fsm.mission_stage == STAGE_SHALLOW) {
                if (current_depth < nominal_target - (HOVER_RECOVERY_M * 1.5f)) hard_drift = true;
            }
        }

        if (hard_drift) {
            printf("!! Control: Hard drift detected (%.2fm). Re-entering TRANSIT.\n", fabs(current_depth - nominal_target));
            global_fsm.ctrl_state = CTRL_TRANSIT;
            global_fsm.filtered_velocity = 0.0f;
        }

        // --- STATE MACHINE UPDATE ---
        if (global_fsm.ctrl_state == CTRL_TRANSIT) { // CTRL_TRANSIT
            // 1. Actuate for transit direction
            if (depth_error < 0.0f) {
                // Too shallow (need to dive)
                global_fsm.actuator_target = settings.act_min;
            } else {
                // Too deep (need to rise)
                global_fsm.actuator_target = settings.act_max;
            }

            // Run bang-bang / braking transition
            bool trigger_braking = false;
            if (global_fsm.mission_stage == STAGE_DEEP) { // Diving stage
                if (-depth_error <= global_fsm.filtered_velocity * settings.kp) {
                    trigger_braking = true;
                }
            } else if (global_fsm.mission_stage == STAGE_SHALLOW) { // Rising stage
                if (depth_error <= -global_fsm.filtered_velocity * settings.kp) {
                    trigger_braking = true;
                }
            }
            
            if (trigger_braking && global_fsm.mission_stage != STAGE_EXITING) {
                printf(">> Control: Transit -> BRAKING (Vel: %.3f m/s, Err: %.2f m)\n", global_fsm.filtered_velocity, depth_error);
                global_fsm.ctrl_state = CTRL_BRAKING;
            }
        }
        else if (global_fsm.ctrl_state == CTRL_BRAKING) { // CTRL_BRAKING
            // Command active counter-buoyancy
            if (global_fsm.mission_stage == STAGE_DEEP) {
                // Diving: apply positive buoyancy to slow down
                global_fsm.actuator_target = global_fsm.active_neutral_adc + (int)settings.ki;
            } else if (global_fsm.mission_stage == STAGE_SHALLOW) {
                // Rising: apply negative buoyancy to slow down
                global_fsm.actuator_target = global_fsm.active_neutral_adc - (int)settings.ki;
            }

            // Check if vertical speed has dropped near zero inside arrival band
            if (fabs(global_fsm.filtered_velocity) <= 0.02f && fabs(current_depth - nominal_target) <= settings.arrival_band_m) {
                printf(">> Control: Braking -> HOVER (Target reached and stopped. Depth: %.2f m)\n", current_depth);
                global_fsm.ctrl_state = CTRL_HOVER;
                global_fsm.last_nudge_time = now;
            }
        }
        else if (global_fsm.ctrl_state == CTRL_HOVER) { // CTRL_HOVER
            // Check for drifts and apply nudges
            bool too_deep = false;
            bool too_shallow = false;

            if (global_fsm.mission_stage == STAGE_SHALLOW) {
                // Asymmetric shallow band: drift down to 0.65m, drift up to 0.42m
                if (current_depth > HOVER_ASYMM_SHALLOW_UP) too_deep = true;
                if (current_depth < HOVER_ASYMM_SHALLOW_DOWN) too_shallow = true;
            } else {
                // Standard symmetric band: +/- settings.kd around target
                if (depth_error > settings.kd) too_deep = true;
                if (depth_error < -settings.kd) too_shallow = true;
            }

            if (too_deep || too_shallow) {
            if (now - global_fsm.last_nudge_time >= NUDGE_WAIT_MS) {
                    if (too_deep) {
                        global_fsm.active_neutral_adc += NUDGE_STEP_ADC;
                        printf(">> Control: Too Deep. Nudging Neutral Up -> %u\n", global_fsm.active_neutral_adc);
                    } else if (too_shallow) {
                        global_fsm.active_neutral_adc -= NUDGE_STEP_ADC;
                        printf(">> Control: Too Shallow. Nudging Neutral Down -> %u\n", global_fsm.active_neutral_adc);
                    }

                    // Clamp learned neutral point
                    if (global_fsm.active_neutral_adc > settings.neutral_buoyancy_adc + 500) {
                        global_fsm.active_neutral_adc = settings.neutral_buoyancy_adc + 500;
                    }
                    if (global_fsm.active_neutral_adc < settings.neutral_buoyancy_adc - 500) {
                        global_fsm.active_neutral_adc = settings.neutral_buoyancy_adc - 500;
                    }

                    global_fsm.last_nudge_time = now;
                }
            }

            global_fsm.actuator_target = global_fsm.active_neutral_adc;
        }
      }
      
      if (prev_state == FLOAT_PROFILING && global_fsm.state == FLOAT_PROFILE_DONE) {
          printf(">> [STORAGE] Profile completed. Saving learned settings to Flash...\n");
          storage_save();
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

    global_fsm.current_actuator_pos = act.cached_pos;
    float_fsm_update(&global_fsm);

    {
      uint32_t ints = save_and_disable_interrupts();
      bool radio_event = radio_event_flag;
      radio_event_flag = false;
      restore_interrupts(ints);
      if (radio_event) {
        float_fsm_process_event(&global_fsm);
      }
    }
    reflash_target_tick(radio_get_instance());

#ifdef HIL_MODE
    static uint32_t last_hil_print_time = 0;
    if (now - last_hil_print_time >= 100) { // 10Hz
        printf("[HIL_OUT] Target=%d ADC=%d State=%d Stage=%d Depth=%.3f\n",
               global_fsm.actuator_target,
               global_fsm.current_actuator_pos,
               global_fsm.state,
               global_fsm.mission_stage,
               global_fsm.current_depth);
        last_hil_print_time = now;
    }
#endif

    sleep_ms(1);
  }
  return 0;
}
