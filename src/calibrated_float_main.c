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
#include "hw_config.h"
#include "hw_init.h"

// Dynamic VREF Settings
#define PWM_WRAP 65535

static float_fsm_t global_fsm;

void onInterrupt(void) { float_fsm_on_interrupt(&global_fsm); }

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

  // --- Initialize Radio ---
  if (!radio_setup_init(onInterrupt)) {
    printf("Radio init failed! Halting.\n");
    while (true) sleep_ms(1000);
  }

  // --- Initialize State Machine ---
  float_fsm_init(&global_fsm, &depth_sensor);

  printf("Float Ready.\n");
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
    if (now - last_act_update >= ACT_LOOP_MS) {
        actuator_set_target(&act, global_fsm.actuator_target);
        actuator_tick(&act); 
        last_act_update = now;
    }

    float_fsm_update(&global_fsm);
    sleep_ms(1);
  }

  return 0;
}