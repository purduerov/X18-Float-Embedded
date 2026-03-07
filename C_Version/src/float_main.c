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
#define MISSION_HOLD_TIME_MS 2000    // TEST MODE: 2s
#define TEST_TRANSITION_MS   5000    // Simulate 5s of travel between depths
#define SAMPLE_INTERVAL_MS   1000
#define MAX_MISSION_PACKETS  300 

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
    FLOAT_PRE_DIVE_TX,
    FLOAT_P1_DESCENDING,
    FLOAT_P1_HOLD_LOW,
    FLOAT_P1_ASCENDING,
    FLOAT_P1_HOLD_HIGH,
    FLOAT_MISSION_DONE,
    FLOAT_DUMPING_DATA
} FloatState_t;

const char *FloatStateNames[] = {
    "IDLE", "PRE_DIVE_TX", "P1_DESCENDING", "P1_HOLD_LOW", "P1_ASCENDING", "P1_HOLD_HIGH", "MISSION_DONE", "DUMPING_DATA"
};

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
    uint32_t stateStartTime = 0; // Renamed from holdStartTime for general usage
    uint32_t lastTxTime = 0;
    uint32_t lastDebugPrint = 0;
    uint16_t currentDumpIndex = 0;

    RadioLib_SX127x_StartReceive(&lora);

    printf("--- Float Surface Test Started ---\n");

    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // 0. PERIODIC DEBUG INFO
        if (now - lastDebugPrint >= 2000) {
            printf("[DEBUG] State: %s | Samples: %u | Mission Time: %lu ms\n", 
                   FloatStateNames[state], mission_sample_count, now - stateStartTime);
            lastDebugPrint = now;
        }

        // 1. DATA SAMPLING (Silent during mission)
        if (state >= FLOAT_P1_DESCENDING && state <= FLOAT_P1_HOLD_HIGH) {
            if (now - lastSampleTime >= SAMPLE_INTERVAL_MS && mission_sample_count < MAX_MISSION_PACKETS) {
                ms5837_read(&depth_sensor);
                mission_depths[mission_sample_count++] = ms5837_get_depth(&depth_sensor) - SENSOR_TOP_OFFSET;
                lastSampleTime = now;
            }
        }

        // 2. MISSION LOGIC (TIMING-BASED FOR SURFACE TEST)
        switch (state) {
            case FLOAT_PRE_DIVE_TX:
                if (!currentlyTransmitting && now - lastTxTime >= 1000) {
                    packet_t tx_pkt = {.command = CMD_PREDIVE_READY};
                    currentlyTransmitting = true;
                    RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                    lastTxTime = now;
                    state = FLOAT_P1_DESCENDING;
                    stateStartTime = now;
                    printf(">> Transitioning to: P1_DESCENDING (Simulated)\n");
                }
                break;

            case FLOAT_P1_DESCENDING:
                // Wait for simulated travel time
                if (now - stateStartTime >= TEST_TRANSITION_MS) {
                    state = FLOAT_P1_HOLD_LOW;
                    stateStartTime = now;
                    printf(">> Transitioning to: P1_HOLD_LOW\n");
                }
                break;

            case FLOAT_P1_HOLD_LOW:
                // Wait for the mission hold duration
                if (now - stateStartTime >= MISSION_HOLD_TIME_MS) {
                    state = FLOAT_P1_ASCENDING;
                    stateStartTime = now;
                    printf(">> Transitioning to: P1_ASCENDING (Simulated)\n");
                }
                break;

            case FLOAT_P1_ASCENDING:
                if (now - stateStartTime >= TEST_TRANSITION_MS) {
                    state = FLOAT_P1_HOLD_HIGH;
                    stateStartTime = now;
                    printf(">> Transitioning to: P1_HOLD_HIGH\n");
                }
                break;

            case FLOAT_P1_HOLD_HIGH:
                if (now - stateStartTime >= MISSION_HOLD_TIME_MS) {
                    state = FLOAT_MISSION_DONE;
                    stateStartTime = now;
                    printf(">> Transitioning to: MISSION_DONE\n");
                }
                break;

            case FLOAT_MISSION_DONE:
                if (!currentlyTransmitting && now - lastTxTime >= 3000) {
                    packet_t tx_pkt = {.command = CMD_DONE_PROFILE};
                    currentlyTransmitting = true;
                    RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                    lastTxTime = now;
                }
                break;

            case FLOAT_DUMPING_DATA:
                if (!currentlyTransmitting && now - lastTxTime >= 1000) {
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
                        stateStartTime = now;
                        mission_sample_count = 0;
                        printf(">> Received BEGIN_PROFILE command.\n");
                    } else if (rx_pkt.command == CMD_SEND_DATA && state == FLOAT_MISSION_DONE) {
                        state = FLOAT_DUMPING_DATA;
                        currentDumpIndex = 0;
                        printf(">> Received SEND_DATA command. Starting Dump.\n");
                    } else if (rx_pkt.command == CMD_ACK && state == FLOAT_DUMPING_DATA) {
                        if (rx_pkt.seq_num == currentDumpIndex + 1) {
                            currentDumpIndex++;
                            if (currentDumpIndex >= mission_sample_count) {
                                state = FLOAT_IDLE;
                                packet_t finish = {.command = CMD_DATA_DONE};
                                currentlyTransmitting = true;
                                RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&finish, sizeof(packet_t));
                                printf(">> All Data Acknowledged. Mission Complete.\n");
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