#include "pico/stdlib.h"
#include "hardware/sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Modular Library Includes ---
#include "data_logger.h"
#include "hw_config.h"
#include "sw_config.h"
#include "hw_init.h"
#include "packets.h"
#include "radio_setup.h"
#include "reflash_host.h"
#include "surface_fsm.h"
#include "console.h"

// --- Global State ---
static surface_fsm_t global_fsm;
static volatile bool radio_event_flag = false;

void onInterrupt(void) { radio_event_flag = true; }

// --- Dashboard Command Handlers ---

static void handle_profile(const char *params) {
  surface_fsm_cmd_begin_profile(&global_fsm);
}

static void handle_pid(const char *params) {
  float p, i, d;
  if (sscanf(params, "%f %f %f", &p, &i, &d) == 3) {
    surface_fsm_cmd_set_pid(&global_fsm, p, i, d);
  }
}

static void handle_company(const char *params) {
  unsigned int id;
  if (sscanf(params, "%u", &id) == 1) {
    surface_fsm_cmd_set_company(&global_fsm, (uint16_t)id);
  }
}

static void handle_duration(const char *params) {
  unsigned int seconds;
  if (sscanf(params, "%u", &seconds) == 1) {
    surface_fsm_cmd_set_duration(&global_fsm, (uint16_t)seconds);
  }
}

static void handle_deep_target(const char *params) {
  float depth;
  if (sscanf(params, "%f", &depth) == 1) {
    surface_fsm_cmd_set_deep_target(&global_fsm, depth);
  }
}

static void handle_shallow_target(const char *params) {
  float depth;
  if (sscanf(params, "%f", &depth) == 1) {
    surface_fsm_cmd_set_shallow_target(&global_fsm, depth);
  }
}

static void handle_num_profiles(const char *params) {
  unsigned int num;
  if (sscanf(params, "%u", &num) == 1) {
    surface_fsm_cmd_set_num_profiles(&global_fsm, (uint16_t)num);
  }
}

static void handle_tolerance(const char *params) {
  float tolerance;
  if (sscanf(params, "%f", &tolerance) == 1) {
    surface_fsm_cmd_set_tolerance(&global_fsm, tolerance);
  }
}

static void handle_zero(const char *params) {
  surface_fsm_cmd_zero_depth(&global_fsm);
}

static void handle_actuator(const char *params) {
  unsigned int pos;
  if (sscanf(params, "%u", &pos) == 1) {
    surface_fsm_cmd_set_actuator(&global_fsm, (uint16_t)pos);
  }
}

static void handle_company_range(const char *params) {
  unsigned int min_val, max_val;
  if (sscanf(params, "%u %u", &min_val, &max_val) == 2) {
    surface_fsm_cmd_set_act_bounds(&global_fsm, (uint16_t)min_val,
                                   (uint16_t)max_val);
  }
}

static void handle_neutral_adc(const char *params) {
  unsigned int val;
  if (sscanf(params, "%u", &val) == 1) {
    surface_fsm_cmd_set_neutral_adc(&global_fsm, (uint16_t)val);
  }
}

static void handle_sync(const char *params) {
  surface_fsm_cmd_sync(&global_fsm);
}

static void handle_reset(const char *params) {
  surface_fsm_cmd_reset(&global_fsm);
}

static void handle_test(const char *params) {
  surface_fsm_cmd_test_mode(&global_fsm);
}

static const console_command_t cmd_table[] = {
    {'p', handle_profile, "Begin Profile"},
    {'s', handle_pid, "Set PID (P I D)"},
    {'c', handle_company, "Set Company ID"},
    {'t', handle_duration, "Set Duration (Secs)"},
    {'d', handle_deep_target, "Set Deep Target Depth (m)"},
    {'u', handle_shallow_target, "Set Shallow Target Depth (m)"},
    {'m', handle_num_profiles, "Set Number of Profiles"},
    {'v', handle_tolerance, "Set Arrival Tolerance (m)"},
    {'z', handle_zero, "Zero Depth"},
    {'a', handle_actuator, "Set Actuator Position (0-4095)"},
    {'b', handle_company_range, "Set Actuator Bounds (Min Max)"},
    {'n', handle_neutral_adc, "Set Neutral ADC Position"},
    {'?', handle_sync, "Sync Settings"},
    {'r', handle_reset, "Reset State Machine"},
    {'k', handle_test, "Enter Test Mode"}};

// --- Main Application ---

int main() {
  // Use unified system init (handles stdio, data_logger, radio, etc)
  if (!system_init(onInterrupt, NULL)) {
    printf("[FATAL] System Init Failed. Radio SX1276 missing?\n");
    while (true) {
        printf("Halting...\n");
        sleep_ms(2000);
    }
  }

  surface_fsm_init(&global_fsm);
  console_init(cmd_table, sizeof(cmd_table) / sizeof(console_command_t));

  printf("Surface Station Ready.\n");
  printf("Commands: 'p' (Profile), 's <P> <I> <D>' (PID), 'c <ID>' (Company), "
         "'t <Sec>' (Time), 'd <m>' (Deep), 'u <m>' (Shallow), 'm <#>' (Count), 'z' (Zero Depth), '?' (Sync)\n");

  uint32_t lastDebugPrint = to_ms_since_boot(get_absolute_time());

  radio_start_receive();

  while (true) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // 1. Periodic Debug Info
    if (now - lastDebugPrint >= SURFACE_DEBUG_INTERVAL_MS) {
      printf("[DEBUG] State: %s | Transmitting: %d | IRQ Flag: %d\n",
             surface_fsm_get_state_name(&global_fsm),
             surface_fsm_is_transmitting(&global_fsm), radio_event_flag);
      lastDebugPrint = now;
    }

    // 2. Process Serial Interface (Commands from Dashboard)
    int c = getchar_timeout_us(0);
    while (c != PICO_ERROR_TIMEOUT) {
      if (c == 'S') {
        reflash_host_stream_from_serial(radio_get_instance());
      } else {
        // Forward everything else to the console character processor
        console_handle_char((char)c);
      }
      c = getchar_timeout_us(0);
    }

    // 3. Process Radio Interface (Packets and IRQs)
    {
      uint32_t ints = save_and_disable_interrupts();
      bool radio_event = radio_event_flag;
      radio_event_flag = false;
      restore_interrupts(ints);
      if (radio_event) {
        surface_fsm_process_event(&global_fsm);
      }
    }

    sleep_ms(1);
  }
  return 0;
}