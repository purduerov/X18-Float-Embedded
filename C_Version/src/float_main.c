#include "pico/stdlib.h"
#include "radiolib_hal_pico.h"
#include "radiolib_sx1276.h"
#include "hardware/i2c.h"
#include "ms5837.h"
#include <stdio.h>
#include <string.h>

// --- Hardware Setup ---
const uint32_t SPI_MOSI = 19;
const uint32_t SPI_MISO = 20;
const uint32_t SPI_SCK = 18;
const uint32_t CS_PIN = 24;
const uint32_t RST_PIN = 25;
const uint32_t EN_PIN = 8;
const uint32_t IRQ_PIN = 9;

#define I2C_PORT i2c1
#define PIN_SDA 2
#define PIN_SCL 3
#define SENSOR_TOP_OFFSET 0.465f

// --- MATE 2026 Mission Parameters ---
#define MISSION_HOLD_TIME_MS 2000  // TEST MODE: 2s (Set to 30000 for competition)
#define TARGET_DEPTH_LOW     2.50f
#define TARGET_DEPTH_HIGH    0.40f
#define DEPTH_TOLERANCE      0.33f
#define SAMPLE_INTERVAL_MS   1000
#define MAX_MISSION_PACKETS  300 // Ample space for two profiles in RAM

typedef enum {
    CMD_NONE = 0x00, CMD_SEND_DATA = 0x01, CMD_DATA_TRANSMISSION = 0x02,
    CMD_SET_PID = 0x03, CMD_BEGIN_PROFILE = 0x04, CMD_DONE_PROFILE = 0x05,
    CMD_DATA_DONE = 0x06, CMD_ACK = 0x07, CMD_PREDIVE_READY = 0x08
} PacketCommand_t;

typedef struct __attribute__((packed)) {
    uint8_t command;
    uint16_t seq_num;
    uint8_t payload[12]; 
} packet_t;

typedef enum {
    FLOAT_IDLE,
    FLOAT_PRE_DIVE_TX,    // REQUIRED: TX before descent
    FLOAT_P1_DESCENDING,
    FLOAT_P1_HOLD_LOW,    // 30s at 2.5m
    FLOAT_P1_ASCENDING,
    FLOAT_P1_HOLD_HIGH,   // 30s at 0.4m
    FLOAT_P2_DESCENDING,
    FLOAT_P2_HOLD_LOW,
    FLOAT_P2_ASCENDING,
    FLOAT_P2_HOLD_HIGH,
    FLOAT_MISSION_DONE,   // Silent until recovery
    FLOAT_DUMPING_DATA    // Autonomous dump after recovery
} FloatState_t;

const char *FloatStateNames[] = {
    "IDLE", "PRE_DIVE_TX", "P1_DESCENDING", "P1_HOLD_LOW", "P1_ASCENDING", "P1_HOLD_HIGH",
    "P2_DESCENDING", "P2_HOLD_LOW", "P2_ASCENDING", "P2_HOLD_HIGH", "MISSION_DONE", "DUMPING_DATA"
};

// Data Storage for the entire mission
float mission_depths[MAX_MISSION_PACKETS];
uint16_t mission_sample_count = 0;

volatile bool operationDoneFlag = false;
void onInterrupt(void) { operationDoneFlag = true; }

int main() {
    stdio_init_all();
    gpio_init(EN_PIN); gpio_set_dir(EN_PIN, GPIO_OUT); gpio_put(EN_PIN, 1);
    sleep_ms(100);

    // Initialize Sensors & Radio
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C); gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    MS5837_t depth_sensor; ms5837_init_struct(&depth_sensor);
    ms5837_begin(&depth_sensor, I2C_PORT, MS5837_02BA);

    RadioLibHal_t *hal = RadioLib_Pico_Create(spi0, SPI_SCK, SPI_MOSI, SPI_MISO, 8000000);
    RadioLibModule_t radioModule; memset(&radioModule, 0, sizeof(RadioLibModule_t));
    RadioLib_Module_Create(&radioModule, hal, CS_PIN, IRQ_PIN, RST_PIN, RADIOLIB_NC);
    RadioLibSX127x_t lora; RadioLib_SX127x_Create(&lora, &radioModule);
    RadioLib_SX1276_Begin(&lora, 915.0, 125.0, 7, 10);
    RadioLib_SX127x_SetAction(&lora, onInterrupt);

    FloatState_t state = FLOAT_IDLE;
    bool currentlyTransmitting = false;
    uint8_t buffer[256];
    uint32_t lastSampleTime = 0;
    uint32_t holdStartTime = 0;
    uint32_t lastTxTime = 0;
    uint16_t currentDumpIndex = 0;

    RadioLib_SX127x_StartReceive(&lora);

    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // 1. DATA SAMPLING (Silent during mission)
        if (state >= FLOAT_P1_DESCENDING && state <= FLOAT_P2_HOLD_HIGH) {
            if (now - lastSampleTime >= SAMPLE_INTERVAL_MS && mission_sample_count < MAX_MISSION_PACKETS) {
                ms5837_read(&depth_sensor);
                mission_depths[mission_sample_count++] = ms5837_get_depth(&depth_sensor) - SENSOR_TOP_OFFSET;
                lastSampleTime = now;
            }
        }

        // 2. MISSION LOGIC & STATE TRANSITIONS
        float current_depth = mission_depths[mission_sample_count > 0 ? mission_sample_count - 1 : 0];
        
        switch (state) {
            case FLOAT_PRE_DIVE_TX:
                if (!currentlyTransmitting && now - lastTxTime >= 1000) {
                    packet_t tx_pkt = {.command = CMD_PREDIVE_READY, .seq_num = 0};
                    currentlyTransmitting = true;
                    RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                    lastTxTime = now;
                    // Auto-transition to descent after one packet
                    state = FLOAT_P1_DESCENDING;
                    printf(">> Starting Profile 1 Descent...\n");
                }
                break;

            case FLOAT_P1_DESCENDING:
                if (current_depth >= (TARGET_DEPTH_LOW - DEPTH_TOLERANCE)) {
                    state = FLOAT_P1_HOLD_LOW;
                    holdStartTime = now;
                    printf(">> Target 2.5m reached. Starting hold timer.\n");
                }
                break;

            case FLOAT_P1_HOLD_LOW:
                if (current_depth < (TARGET_DEPTH_LOW - DEPTH_TOLERANCE) || current_depth > (TARGET_DEPTH_LOW + DEPTH_TOLERANCE)) {
                    holdStartTime = now; // Reset timer if out of range
                } else if (now - holdStartTime >= MISSION_HOLD_TIME_MS) {
                    state = FLOAT_P1_ASCENDING;
                    printf(">> 2.5m hold complete. Ascending to 40cm...\n");
                }
                break;

            case FLOAT_P1_ASCENDING:
                if (current_depth <= (TARGET_DEPTH_HIGH + DEPTH_TOLERANCE)) {
                    state = FLOAT_P1_HOLD_HIGH;
                    holdStartTime = now;
                    printf(">> Target 40cm reached. Starting hold timer.\n");
                }
                break;

            case FLOAT_P1_HOLD_HIGH:
                if (current_depth < (TARGET_DEPTH_HIGH - DEPTH_TOLERANCE) || current_depth > (TARGET_DEPTH_HIGH + DEPTH_TOLERANCE)) {
                    holdStartTime = now;
                } else if (now - holdStartTime >= MISSION_HOLD_TIME_MS) {
                    state = FLOAT_MISSION_DONE; // Shortened for your requested 2-profile test logic
                    printf(">> Profile 1 complete. In a real mission, I would repeat for P2.\n");
                }
                break;

            case FLOAT_MISSION_DONE:
                if (!currentlyTransmitting && now - lastTxTime >= 3000) {
                    packet_t tx_pkt = {.command = CMD_DONE_PROFILE, .seq_num = 0};
                    currentlyTransmitting = true;
                    RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                    lastTxTime = now;
                }
                break;

            case FLOAT_DUMPING_DATA:
                if (!currentlyTransmitting && now - lastTxTime >= 1500) {
                    packet_t tx_pkt = {.command = CMD_DATA_TRANSMISSION, .seq_num = currentDumpIndex + 1};
                    float payload[3] = {mission_depths[currentDumpIndex], 0.0f, 0.0f};
                    memcpy(tx_pkt.payload, payload, 12);
                    currentlyTransmitting = true;
                    RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                    lastTxTime = now;
                }
                break;
        }

        // 3. INTERRUPT & RADIO HANDLING
        if (operationDoneFlag) {
            operationDoneFlag = false;
            if (currentlyTransmitting) {
                RadioLib_SX127x_FinishTransmit(&lora);
                currentlyTransmitting = false;
                RadioLib_SX127x_StartReceive(&lora);
            } else {
                int16_t len = RadioLib_SX127x_ReadData(&lora, buffer, sizeof(buffer));
                if (len == sizeof(packet_t)) {
                    packet_t rx_pkt; memcpy(&rx_pkt, buffer, sizeof(packet_t));
                    if (rx_pkt.command == CMD_BEGIN_PROFILE && state == FLOAT_IDLE) {
                        state = FLOAT_PRE_DIVE_TX;
                        mission_sample_count = 0;
                    } else if (rx_pkt.command == CMD_SEND_DATA && state == FLOAT_MISSION_DONE) {
                        state = FLOAT_DUMPING_DATA;
                        currentDumpIndex = 0;
                    } else if (rx_pkt.command == CMD_ACK && state == FLOAT_DUMPING_DATA) {
                        if (rx_pkt.seq_num == currentDumpIndex + 1) {
                            currentDumpIndex++;
                            if (currentDumpIndex >= mission_sample_count) {
                                state = FLOAT_IDLE; // Mission Fully Complete
                                packet_t finish = {.command = CMD_DATA_DONE};
                                currentlyTransmitting = true;
                                RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&finish, sizeof(packet_t));
                            }
                        }
                    }
                }
                if (!currentlyTransmitting) RadioLib_SX127x_StartReceive(&lora);
            }
        }
        sleep_ms(1);
    }
}