#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"

// Library Headers
#include "ms5837.h"
#include "radiolib_sx1276.h"
#include "radiolib_hal_pico.h"
#include "packets.h"

// --- Hardware Configuration ---
#define I2C_PORT i2c1
#define PIN_SDA 2
#define PIN_SCL 3

#define SPI_PORT spi0
#define PIN_SCK 18
#define PIN_MOSI 19
#define PIN_MISO 20
#define PIN_CS 24
#define PIN_RST 25
#define PIN_EN 8
#define PIN_IRQ 9

#define SENSOR_TOP_OFFSET 0.0f
#define MAX_CAL_SAMPLES 500
#define SAMPLE_INTERVAL_MS 1000

// --- Data Buffers ---
static float recorded_depths[MAX_CAL_SAMPLES];
static uint32_t recorded_times[MAX_CAL_SAMPLES];
static uint16_t sample_count = 0;

int main() {
    stdio_init_all();
    
    // 1. HARDWARE WAKEUP
    gpio_init(PIN_EN);
    gpio_set_dir(PIN_EN, GPIO_OUT);
    gpio_put(PIN_EN, 1); 
    sleep_ms(20); 
    
    uint32_t waitTime = 0;
    while (!stdio_usb_connected() && waitTime < 5000) {
        sleep_ms(100);
        waitTime += 100;
    }
    printf("\n--- MS5837 Depth Calibration (Buffered) ---\n");

    // 2. I2C INITIALIZATION
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);

    // 3. MS5837 INIT
    MS5837_t depth_sensor;
    ms5837_init_struct(&depth_sensor);
    if (!ms5837_begin(&depth_sensor, I2C_PORT, MS5837_02BA)) {
        printf("CRITICAL ERROR: MS5837 FAILED\n");
    }

    // 4. RADIOLIB SETUP
    RadioLibHal_t *hal = RadioLib_Pico_Create(SPI_PORT, PIN_SCK, PIN_MOSI, PIN_MISO, 8000000);
    RadioLibModule_t mod;
    memset(&mod, 0, sizeof(RadioLibModule_t));
    RadioLib_Module_Create(&mod, hal, PIN_CS, PIN_IRQ, PIN_RST, RADIOLIB_NC);
    mod.enPin = PIN_EN; 
    for (int i = 0; i < 6; i++) mod.radioGPins[i] = RADIOLIB_NC;

    RadioLibSX127x_t lora;
    RadioLib_SX127x_Create(&lora, &mod);
    int16_t radio_state = RadioLib_SX1276_Begin(&lora, 915.0, 125.0, 7, 10);
    
    if (radio_state != RADIOLIB_ERR_NONE) {
        printf("Radio FAILED: %d\n", radio_state);
    }

    uint32_t last_sample_time = to_ms_since_boot(get_absolute_time());

    printf("\nCalibration starting. Storing 1Hz samples (Max %d)...\n", MAX_CAL_SAMPLES);
    printf("Send CMD_SEND_DATA (0x01) to dump results.\n\n");

    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // A. Sample depth at 1Hz
        if (now - last_sample_time >= SAMPLE_INTERVAL_MS && sample_count < MAX_CAL_SAMPLES) {
            ms5837_read(&depth_sensor);
            recorded_depths[sample_count] = ms5837_get_depth(&depth_sensor) - SENSOR_TOP_OFFSET;
            recorded_times[sample_count] = now;
            sample_count++;
            last_sample_time = now;
            if (sample_count % 10 == 0) printf("Samples recorded: %d\n", sample_count);
        }

        // B. Listen for "SEND_DATA" command
        if (radio_state == RADIOLIB_ERR_NONE) {
            uint8_t rx_buffer[sizeof(packet_t)];
            // Non-blocking check for radio data
            int16_t rx_state = RadioLib_SX127x_Receive(&lora, rx_buffer, sizeof(packet_t));
            
            if (rx_state == RADIOLIB_ERR_NONE) {
                packet_t *pkt = (packet_t *)rx_buffer;
                if (pkt->command == CMD_SEND_DATA) {
                    printf("Received SEND_DATA. Dumping %d samples...\n", sample_count);
                    
                    for (uint16_t i = 0; i < sample_count; i++) {
                        packet_t tx_pkt = {
                            .command = CMD_DATA_TRANSMISSION,
                            .seq_num = i + 1
                        };
                        tx_pkt.payload.telemetry.time_ms = recorded_times[i];
                        tx_pkt.payload.telemetry.depth_m = recorded_depths[i];
                        tx_pkt.checksum = packet_calculate_checksum(&tx_pkt);

                        RadioLib_SX127x_Transmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                        printf("Sent %d/%d: %.3f m\n", i + 1, sample_count, recorded_depths[i]);
                        sleep_ms(100); // Small delay to avoid flooding receiver
                    }
                    
                    packet_t done_pkt = {.command = CMD_DATA_DONE, .seq_num = sample_count};
                    done_pkt.checksum = packet_calculate_checksum(&done_pkt);
                    RadioLib_SX127x_Transmit(&lora, (uint8_t *)&done_pkt, sizeof(packet_t));
                    
                    printf("Dump complete. Clearing buffer.\n");
                    sample_count = 0;
                }
            }
        }
        
        sleep_ms(10);
    }

    return 0;
}
