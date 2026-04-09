#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

// Define where to store data (the last sector of flash)
// RP2040 Flash is usually 2MB. We use the very last 4KB sector.
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define SETTINGS_MAGIC 0xDEADBEEF

// Struct to hold our PID values
typedef struct {
    float kp;
    float ki;
    float kd;
    uint32_t magic_number; 
} pid_settings_t;

pid_settings_t current_settings;

// Save current_settings from RAM to Flash
void save_settings() {
    printf("Writing to Flash...\n");
    
    // Flash programming must be done in page sizes (256 bytes)
    uint8_t buffer[FLASH_PAGE_SIZE];
    memset(buffer, 0, FLASH_PAGE_SIZE);
    memcpy(buffer, &current_settings, sizeof(pid_settings_t));

    // Disable interrupts to prevent the CPU from trying to read code 
    // from flash while we are erasing/writing to it.
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
    
    printf("Flash Save Complete.\n");
}

// Load settings from Flash into RAM
void load_settings() {
    // XIP_BASE is the memory address where Flash is mirrored
    const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    memcpy(&current_settings, flash_target_contents, sizeof(pid_settings_t));

    // Check if we have valid data by looking for the magic number
    if (current_settings.magic_number != SETTINGS_MAGIC) {
        printf("No saved settings found. Initializing defaults.\n");
        current_settings.kp = 1.0f;
        current_settings.ki = 0.0f;
        current_settings.kd = 0.0f;
        current_settings.magic_number = SETTINGS_MAGIC;
        save_settings(); // Save defaults immediately
    } else {
        printf("Successfully loaded settings from Flash.\n");
    }
}

int littlefs_main() {
    // Initialize all standard I/O (USB and UART)
    stdio_init_all();

    // Use the built-in LED on the Feather RP2040 (GPIO 13) to show status
    const uint LED_PIN = 13;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Wait for the Serial Monitor to be opened before continuing
    // The LED will blink while waiting.
    while (!stdio_usb_connected()) {
        gpio_put(LED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_PIN, 0);
        sleep_ms(100);
    }

    printf("\n\n======================================\n");
    printf("   RP2040 PERSISTENT PID STORAGE\n");
    printf("======================================\n");

    load_settings();

    printf("Current PID: P:%.2f I:%.2f D:%.2f\n", 
            current_settings.kp, current_settings.ki, current_settings.kd);
    printf("Update Command: P [kp] [ki] [kd] (e.g., P 1.5 0.5 0.01)\n");
    printf("> ");
    fflush(stdout);

    char input_buf[64];
    int idx = 0;

    while (true) {
        // Non-blocking check for serial input
        int c = getchar_timeout_us(0);
        
        if (c != PICO_ERROR_TIMEOUT) {
            // Handle Enter key (\r or \n)
            if (c == '\r' || c == '\n') {
                input_buf[idx] = '\0';
                
                if (idx > 0) {
                    if (input_buf[0] == 'P' || input_buf[0] == 'p') {
                        float p, i, d;
                        int parsed = sscanf(input_buf, "P %f %f %f", &p, &i, &d);
                        
                        // If it failed, try lowercase 'p'
                        if (parsed != 3) {
                            parsed = sscanf(input_buf, "p %f %f %f", &p, &i, &d);
                        }

                        if (parsed == 3) {
                            current_settings.kp = p;
                            current_settings.ki = i;
                            current_settings.kd = d;
                            save_settings();
                            printf("\n[SUCCESS] New PID: P:%.2f I:%.2f D:%.2f\n", p, i, d);
                        } else {
                            printf("\n[ERROR] Could not parse values. Found %d/3.\n", parsed);
                            printf("Format: P 1.0 0.5 0.01\n");
                        }
                    }
                    printf("> ");
                    fflush(stdout);
                }
                idx = 0; // Reset buffer
            } 
            // Handle Backspace
            else if (c == 8 || c == 127) {
                if (idx > 0) idx--;
            }
            // Add character to buffer
            else if (idx < 63) {
                input_buf[idx++] = (char)c;
            }
        }

        // Small heartbeat to keep the LED on while running
        gpio_put(LED_PIN, 1);
        sleep_ms(10); 
    }

    return 0;
}