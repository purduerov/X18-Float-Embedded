#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Modular Library Includes ---
#include "packets.h"
#include "radio_setup.h"
#include "surface_link.h"
#include "data_logger.h"
#include "surface_fsm.h"
#include "reflash_host.h"

// --- Global State ---
static surface_fsm_t global_fsm;
static volatile bool operationDoneFlag = false;

void onInterrupt(void) { operationDoneFlag = true; }

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

static void handle_zero(const char *params) {
    surface_fsm_cmd_zero_depth(&global_fsm);
}

static void handle_actuator(const char *params) {
    unsigned int pos;
    if (sscanf(params, "%u", &pos) == 1) {
        surface_fsm_cmd_set_actuator(&global_fsm, (uint16_t)pos);
    }
}

static void handle_bounds(const char *params) {
    unsigned int min_val, max_val;
    if (sscanf(params, "%u %u", &min_val, &max_val) == 2) {
        surface_fsm_cmd_set_act_bounds(&global_fsm, (uint16_t)min_val, (uint16_t)max_val);
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

static const surface_command_t cmd_table[] = {
    {'p', handle_profile, "Begin Profile"},
    {'s', handle_pid, "Set PID (P I D)"},
    {'c', handle_company, "Set Company ID"},
    {'t', handle_duration, "Set Duration (Secs)"},
    {'z', handle_zero, "Zero Depth"},
    {'a', handle_actuator, "Set Actuator Position (0-4095)"},
    {'b', handle_bounds, "Set Actuator Bounds (Min Max)"},
    {'?', handle_sync, "Sync Settings"},
    {'r', handle_reset, "Reset State Machine"},
    {'k', handle_test, "Enter Test Mode"}
};

// --- Main Application ---

int main() {
  stdio_init_all();
  stdio_set_translate_crlf(&stdio_usb, false); // Binary safe
  data_logger_init();
  surface_fsm_init(&global_fsm);

  uint32_t waitTime = 0;
  while (!stdio_usb_connected() && waitTime < 5000) {
    sleep_ms(100);
    waitTime += 100;
  }

  printf("\n\n=== X18 Surface Station Booting (Ultra Modular) ===\n");

  if (!radio_setup_init(onInterrupt)) {
    printf("Radio init failed! Halting.\n");
    while (true) sleep_ms(1000);
  }

  surface_link_init(cmd_table, sizeof(cmd_table) / sizeof(surface_command_t));

  printf("Surface Station Ready.\n");
  printf("Commands: 'p' (Profile), 's <P> <I> <D>' (PID), 'c <ID>' (Company), "
         "'t <Sec>' (Time), 'z' (Zero Depth), '?' (Sync)\n");

  uint32_t lastDebugPrint = to_ms_since_boot(get_absolute_time());

  radio_start_receive();

  while (true) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // 1. Periodic Debug Info
    if (now - lastDebugPrint >= 2000) {
      printf("[DEBUG] State: %s | Transmitting: %d | IRQ Flag: %d\n",
             surface_fsm_get_state_name(&global_fsm), 
             surface_fsm_is_transmitting(&global_fsm),
             operationDoneFlag);
      lastDebugPrint = now;
    }

    // 2. Process Serial Interface (Commands from Dashboard)
    int c = getchar_timeout_us(0);
    while (c != PICO_ERROR_TIMEOUT) {
        if (c == 'S') {
            reflash_host_stream_from_serial(radio_get_instance());
        } else {
            // Forward everything else to the surface_link character processor
            surface_link_handle_char((char)c);
        }
        c = getchar_timeout_us(0);
    }

    // 3. Process Radio Interface (Packets and IRQs)
    if (operationDoneFlag) {
      operationDoneFlag = false;
      surface_fsm_process_event(&global_fsm);
    }

    sleep_ms(1);
  }
  return 0;
}