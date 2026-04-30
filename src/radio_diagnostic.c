#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include "hw_config.h"
#include "radio_setup.h"

static volatile bool radio_event = false;

void on_radio_interrupt(void) {
    radio_event = true;
}

int main() {
    stdio_init_all();
    sleep_ms(3000); // Wait for USB serial

    printf("\n\n--- LoRa Radio Diagnostic ---\n");
#ifdef TARGET_SURFACE
    printf("Mode: SURFACE (Ping)\n");
    printf("Config: CS=%d, RST=%1d, EN=%d, IRQ=%d\n", PIN_CS, PIN_RST, PIN_EN, PIN_IRQ);
#else
    printf("Mode: FLOAT (Pong)\n");
    printf("Config: CS=%d, RST=%d, EN=%d, IRQ=%d\n", PIN_CS, PIN_RST, PIN_EN, PIN_IRQ);
#endif

    if (!radio_setup_init(on_radio_interrupt)) {
        printf("FAILED to initialize radio! Check SPI wiring and pins.\n");
        while (1) {
            tight_loop_contents();
        }
    }

    printf("Radio initialized successfully!\n");

#ifdef TARGET_SURFACE
    // Surface (Ping) Loop
    uint32_t count = 0;
    while (1) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Ping %lu", count++);
        printf("Sending: %s\n", msg);
        
        radio_start_transmit((uint8_t*)msg, strlen(msg));
        
        // Wait for TX completion (IRQ)
        uint32_t start_tx = to_ms_since_boot(get_absolute_time());
        while (!radio_event && (to_ms_since_boot(get_absolute_time()) - start_tx < 1000)) {
            tight_loop_contents();
        }
        
        if (radio_event) {
            radio_event = false;
            radio_finish_transmit();
            printf("Sent successfully!\n");
            
            // Listen for reply
            radio_start_receive();
            uint32_t start_wait = to_ms_since_boot(get_absolute_time());
            bool got_reply = false;
            while (to_ms_since_boot(get_absolute_time()) - start_wait < 1000) {
                if (radio_event) {
                    radio_event = false;
                    uint8_t buffer[64];
                    int16_t len = radio_read_data(buffer, sizeof(buffer));
                    if (len > 0) {
                        buffer[len] = '\0';
                        printf("Got Reply: %s\n", (char*)buffer);
                        got_reply = true;
                        break;
                    }
                }
                tight_loop_contents();
            }
            if (!got_reply) {
                printf("No reply received.\n");
            }
        } else {
            printf("TX Timeout! IRQ pin %d not triggering?\n", PIN_IRQ);
        }

        sleep_ms(2000);
    }
#else
    // Float (Pong) Loop
    radio_start_receive();
    printf("Listening for Pings...\n");
    while (1) {
        if (radio_event) {
            radio_event = false;
            uint8_t buffer[64];
            int16_t len = radio_read_data(buffer, sizeof(buffer));
            if (len > 0) {
                buffer[len] = '\0';
                printf("Received: %s\n", (char*)buffer);
                
                // Send reply
                char reply[32];
                snprintf(reply, sizeof(reply), "Pong %s", (char*)buffer);
                printf("Sending Reply: %s\n", reply);
                
                radio_start_transmit((uint8_t*)reply, strlen(reply));
                
                // Wait for TX completion
                uint32_t start_tx = to_ms_since_boot(get_absolute_time());
                while (!radio_event && (to_ms_since_boot(get_absolute_time()) - start_tx < 1000)) {
                    tight_loop_contents();
                }
                if (radio_event) {
                    radio_event = false;
                    radio_finish_transmit();
                    printf("Reply sent!\n");
                } else {
                    printf("Reply TX Timeout!\n");
                }
                
                // Go back to receiving
                radio_start_receive();
            }
        }
        tight_loop_contents();
    }
#endif

    return 0;
}