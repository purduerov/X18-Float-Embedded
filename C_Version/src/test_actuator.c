#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "actuator.h"

// Hardware Pins
#define PIN_POT 26   
#define PIN_EXT 12   
#define PIN_RET 13   

// Control Settings
#define SETTLE_TIME_MS 150    // Time to wait for noise to clear after stopping motor
#define MIN_PULSE_MS 100      // Smallest possible movement burst
#define MAX_PULSE_MS 1500     // Longest possible movement burst
#define Kp_PULSE 3000.0f      // Proportional gain (3000ms per 1.0 position error)
#define TARGET_PADDING 0.02f  // Success padding (stop if within 2% of target)

int main() {
    stdio_init_all();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    printf("\n--- Actuator DYNAMIC PULSE Test ---\n");

    Actuator test_act;
    actuator_init(&test_act, PIN_POT, PIN_EXT, PIN_RET);

    float targets[] = {0.80f, 0.20f}; 
    int target_idx = 0;

    while (1) {
        float current_target = targets[target_idx];
        
        printf("\n>>> TARGET: %.2f\n", current_target);

        while (1) {
            // 1. STOP and take a CLEAN reading
            actuator_set_move_pins(&test_act, 0);
            sleep_ms(SETTLE_TIME_MS);
            float current_pos = actuator_get_position(&test_act);
            
            float error = current_target - current_pos;
            float abs_error = (error < 0) ? -error : error;

            // 2. Check if we are within the PADDING (Target Reached)
            if (abs_error <= TARGET_PADDING) {
                printf("Target Reached! Final Pos: %.3f\n", current_pos);
                break;
            }

            // 3. Calculate DYNAMIC Pulse Duration
            // Pulse length is proportional to the distance remaining
            int pulse_duration = (int)(abs_error * Kp_PULSE);
            
            // Constrain pulse duration to safe limits
            if (pulse_duration < MIN_PULSE_MS) pulse_duration = MIN_PULSE_MS;
            if (pulse_duration > MAX_PULSE_MS) pulse_duration = MAX_PULSE_MS;

            int dir = (error > 0) ? 1 : -1;
            
            printf("  [Distance: %.3f] Pulsing %dms in direction: %d\n", 
                   abs_error, pulse_duration, dir);

            // 4. PERFORM PULSE (ADC is ignored during this time)
            actuator_set_move_pins(&test_act, dir);
            sleep_ms(pulse_duration);
            
            // Stop motor immediately after pulse
            actuator_set_move_pins(&test_act, 0);
        }

        printf(">>> CYCLE COMPLETE. Cooling down for 2s...\n");
        sleep_ms(2000);
        target_idx = (target_idx + 1) % 2;
    }

    return 0;
}