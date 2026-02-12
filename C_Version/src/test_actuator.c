#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "actuator.h"

// Hardware Pins (Matches your configuration)
#define PIN_POT 26   // ADC0
#define PIN_EXT 12   // Extend
#define PIN_RET 13   // Retract

int main() {
    stdio_init_all();

    // Wait for serial to connect
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    printf("--- Actuator Individual Component Test ---\n");

    // 1. Initialize Actuator
    Actuator test_act;
    actuator_init(&test_act, PIN_POT, PIN_EXT, PIN_RET);

    // 2. Test ADC / Potentiometer Reading
    printf("\nChecking Potentiometer... Move it manually if possible.\n");
    for(int i = 0; i < 20; i++) {
        float pos = actuator_get_position(&test_act);
        printf("Current Position: %.3f\n", pos);
        sleep_ms(200);
    }

    // 3. Test Manual Motor Control
    printf("\nTesting Motor: EXTENDING for 1 second...\n");
    actuator_set_move_pins(&test_act, 1);
    sleep_ms(1000);
    
    printf("Testing Motor: RETRACTING for 1 second...\n");
    actuator_set_move_pins(&test_act, -1);
    sleep_ms(1000);
    
    printf("Stopping Motor.\n");
    actuator_set_move_pins(&test_act, 0);
    sleep_ms(500);

    // 4. Test "Move To" with "Tick" logic
    // This emulates how the final main loop will work
    float target = 0.75f;
    printf("\nTesting Move-To Logic: Target = %.2f\n", target);
    actuator_move_to(&test_act, target);

    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    
    // Loop until stopped or 5 second timeout
    while (test_act.moving != 0) {
        actuator_tick(&test_act);
        
        // Print progress every 100ms
        if ((to_ms_since_boot(get_absolute_time()) - start_time) % 100 == 0) {
            printf("Moving... Current Pos: %.3f | Target: %.3f\n", 
                   actuator_get_position(&test_act), test_act.move_target);
        }

        if (to_ms_since_boot(get_absolute_time()) - start_time > 5000) {
            printf("TIMEOUT: Actuator failed to reach target in 5s.\n");
            actuator_set_move_pins(&test_act, 0);
            break;
        }
        
        sleep_ms(10); // High frequency tick for accuracy
    }

    printf("Test Complete. Final Position: %.3f\n", actuator_get_position(&test_act));

    while (1) {
        tight_loop_contents();
    }

    return 0;
}