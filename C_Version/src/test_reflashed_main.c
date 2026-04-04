#include "pico/stdlib.h"
#include "radio_setup.h"
#include "reflash_target.h"
#include <stdio.h>
#include <string.h>

// Hardware watchdog is already used inside critical_swap_routine if needed,
// but we should enable it here so the system can recover if it hangs.
#include "hardware/watchdog.h"

int main() {
  stdio_init_all();
  stdio_set_translate_crlf(&stdio_usb, false); // Binary safe

  uint32_t waitTime = 0;
  while (!stdio_usb_connected() && waitTime < 5000) {
    sleep_ms(100);
    waitTime += 100;
  }

  printf("\n\n=== NEW FLASHED CODE WORKS! ===\n");
  printf("This is the test_reflashed_main.c running on the Float.\n");
  printf("Waiting for more OTA updates...\n");

  // Initialize Radio
  if (!radio_setup_init(NULL)) {
    printf("Radio init failed! Halting.\n");
    while (true)
      sleep_ms(1000);
  }

  // Start listening (non-blocking)
  RadioLib_SX127x_StartReceive(radio_get_instance());

  // Enable watchdog
  watchdog_enable(5000, 1);

  uint8_t rx_buffer[512];
  uint32_t last_print_time = to_ms_since_boot(get_absolute_time());

  while (true) {
    watchdog_update();
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (now - last_print_time >= 3000) {
      printf("=== NEW FLASHED CODE WORKS! (Still alive) yeet===\n");
      last_print_time = now;
    }

    // Poll the radio IRQ pin manually (Pin 9)
    if (gpio_get(9)) {
      printf("[DEBUG] IRQ Fired!\n");
      int16_t state = RadioLib_SX127x_ReadData(radio_get_instance(), rx_buffer,
                                               sizeof(rx_buffer));
      if (state > 0) {
        printf("[DEBUG] Read %d bytes, Type: 0x%02X\n", state, rx_buffer[0]);
        if (reflash_target_process_packet(radio_get_instance(), rx_buffer,
                                          (size_t)state)) {
          // Reflash system took over
        } else {
          printf("Received unknown packet type: 0x%02X\n", rx_buffer[0]);
        }
      } else {
        printf("[DEBUG] ReadData returned error code %d\n", state);
      }
      // StartReceive again to clear flags and keep listening
      RadioLib_SX127x_StartReceive(radio_get_instance());
    }

    sleep_ms(1);
  }

  return 0;
}