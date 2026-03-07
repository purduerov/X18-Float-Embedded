#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ms5837.h"
#include <stdio.h>
#include <string.h>

// --- Custom Library Includes ---
#include "packets.h"
#include "storage.h"
#include "radio_setup.h"

// --- Profiling Configuration ---
#define SAMPLE_INTERVAL_MS 1000
#define MAX_PACKETS 400 // Allocates memory for max ~6.5 minutes of data at 1Hz

float recorded_depths[MAX_PACKETS];
uint32_t recorded_times[MAX_PACKETS];

// --- I2C / Sensor Setup ---
#define I2C_PORT i2c1
#define PIN_SDA 2
#define PIN_SCL 3
#define SENSOR_TOP_OFFSET 0.465f

typedef enum
{
    FLOAT_IDLE,
    FLOAT_PRE_DIVE,      
    FLOAT_PROFILING,
    FLOAT_PROFILE_DONE,
    FLOAT_DUMPING_DATA
} FloatState_t;

const char *FloatStateNames[] = {
    "IDLE", "PRE_DIVE", "PROFILING", "PROFILE_DONE", "DUMPING_DATA"};

volatile bool operationDoneFlag = false;
void onInterrupt(void) { operationDoneFlag = true; }

int main()
{
    stdio_init_all();

    uint32_t waitTime = 0;
    while (!stdio_usb_connected() && waitTime < 5000)
    {
        sleep_ms(100);
        waitTime += 100;
    }

    printf("\n\n=== MATE Float Station Booting ===\n");

    // --- Initialize Persistent Storage ---
    printf("Initializing Flash Storage...\n");
    storage_init(); 

    // --- Initialize I2C and MS5837 ---
    printf("Initializing I2C Bus...");
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);
    printf("Done.\n");

    MS5837_t depth_sensor;
    ms5837_init_struct(&depth_sensor);
    if (!ms5837_begin(&depth_sensor, I2C_PORT, MS5837_02BA))
    {
        printf("CRITICAL ERROR: MS5837 FAILED to initialize\n");
    }
    else
    {
        printf("MS5837 Initialized.\n");
    }

    // --- Initialize Radio (Using Shared Library) ---
    if (!radio_setup_init(onInterrupt)) {
        printf("Radio init failed! Halting.\n");
        while (true) sleep_ms(1000);
    }

    printf("Float Initialized. Waiting in IDLE state...\n");

    FloatState_t state = FLOAT_IDLE;
    bool currentlyTransmitting = false;
    uint8_t buffer[256];

    // FSM Timing & Tracking
    uint32_t profileStartTime = 0;
    uint32_t lastTxTime = 0;
    uint32_t lastSampleTime = 0;
    uint16_t currentSeqNum = 1;
    uint16_t sampleIndex = 0;

    uint32_t lastDebugPrint = to_ms_since_boot(get_absolute_time());

    RadioLib_SX127x_StartReceive(&lora);

    while (true)
    {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (now - lastDebugPrint >= 2000)
        {
            printf("[DEBUG] State: %s | Transmitting: %d | IRQ Flag: %d\n",
                   FloatStateNames[state], currentlyTransmitting, operationDoneFlag);
            lastDebugPrint = now;
        }

        if (state == FLOAT_PRE_DIVE && !currentlyTransmitting)
        {
            printf(">> Sending Pre-Dive Data Packet...\n");
            
            packet_t tx_pkt = {.command = CMD_DATA_TRANSMISSION, .seq_num = 0};
            tx_pkt.payload.telemetry.company_number = current_settings.company_number;
            tx_pkt.payload.telemetry.time_ms = now;
            tx_pkt.payload.telemetry.depth_m = 0.0f;

            currentlyTransmitting = true;
            RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
        }
        else if (state == FLOAT_PROFILING)
        {
            if (now - lastSampleTime >= SAMPLE_INTERVAL_MS && sampleIndex < MAX_PACKETS)
            {
                ms5837_read(&depth_sensor);
                recorded_depths[sampleIndex] = ms5837_get_depth(&depth_sensor) - SENSOR_TOP_OFFSET;
                recorded_times[sampleIndex] = now;

                printf(">> Sample %u/%u: Time %lu ms | Depth %.2f m\n",
                       sampleIndex + 1, MAX_PACKETS, recorded_times[sampleIndex], recorded_depths[sampleIndex]);

                sampleIndex++;
                lastSampleTime = now;
            }

            // Dynamically check against the duration setting
            if (now - profileStartTime >= (current_settings.profile_duration_s * 1000))
            {
                printf(">> Profile complete (%u sec). Surfacing...\n", current_settings.profile_duration_s);
                state = FLOAT_PROFILE_DONE;
            }
        }
        else if (state == FLOAT_PROFILE_DONE && !currentlyTransmitting)
        {
            if (now - lastTxTime >= 3000)
            {
                printf(">> Broadcasting DONE_PROFILE (Waiting for Recovery / SEND_DATA CMD)...\n");
                packet_t tx_pkt = {.command = CMD_DONE_PROFILE, .seq_num = 0};
                currentlyTransmitting = true;
                RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                lastTxTime = now;
            }
        }
        else if (state == FLOAT_DUMPING_DATA && !currentlyTransmitting)
        {
            if (now - lastTxTime >= 2000)
            {
                printf(">> Sending/Retransmitting Data Packet %d...\n", currentSeqNum);

                packet_t tx_pkt = {.command = CMD_DATA_TRANSMISSION, .seq_num = currentSeqNum};
                tx_pkt.payload.telemetry.company_number = current_settings.company_number;
                tx_pkt.payload.telemetry.time_ms = recorded_times[currentSeqNum - 1];
                tx_pkt.payload.telemetry.depth_m = recorded_depths[currentSeqNum - 1];

                currentlyTransmitting = true;
                RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                lastTxTime = now;
            }
        }

        if (operationDoneFlag)
        {
            operationDoneFlag = false;

            if (currentlyTransmitting)
            {
                RadioLib_SX127x_FinishTransmit(&lora);
                currentlyTransmitting = false;

                if (state == FLOAT_PRE_DIVE) 
                {
                    printf(">> Pre-dive packet sent. Starting dive profiles (Radio SILENT)...\n");
                    state = FLOAT_PROFILING;
                    profileStartTime = to_ms_since_boot(get_absolute_time());
                    lastSampleTime = profileStartTime;
                    sampleIndex = 0;
                }

                RadioLib_SX127x_StartReceive(&lora);
            }
            else
            {
                int16_t len = RadioLib_SX127x_ReadData(&lora, buffer, sizeof(buffer));

                if (len == sizeof(packet_t))
                {
                    packet_t rx_pkt;
                    memcpy(&rx_pkt, buffer, sizeof(packet_t));

                    if (state == FLOAT_IDLE)
                    {
                        if (rx_pkt.command == CMD_BEGIN_PROFILE)
                        {
                            printf(">> Received BEGIN_PROFILE. Triggering Pre-Dive Transmission...\n");
                            state = FLOAT_PRE_DIVE; 
                        }
                        else if (rx_pkt.command == CMD_SET_PID)
                        {
                            current_settings.kp = rx_pkt.payload.settings.kp;
                            current_settings.ki = rx_pkt.payload.settings.ki;
                            current_settings.kd = rx_pkt.payload.settings.kd;
                            printf(">> PID Updated: P=%.2f, I=%.2f, D=%.2f. Saving to Flash...\n", 
                                   current_settings.kp, current_settings.ki, current_settings.kd);
                            storage_save();
                        }
                        else if (rx_pkt.command == CMD_SET_COMPANY)
                        {
                            current_settings.company_number = rx_pkt.payload.telemetry.company_number;
                            printf(">> Company ID Updated: %u. Saving to Flash...\n", current_settings.company_number);
                            storage_save();
                        }
                        else if (rx_pkt.command == CMD_SET_DURATION)
                        {
                            current_settings.profile_duration_s = rx_pkt.payload.settings.profile_duration_s;
                            printf(">> Profile Duration Updated: %u seconds. Saving to Flash...\n", current_settings.profile_duration_s);
                            storage_save();
                        }
                        else if (rx_pkt.command == CMD_REQ_SETTINGS)
                        {
                            printf(">> Received REQ_SETTINGS. Transmitting Flash config back to surface...\n");
                            packet_t tx_pkt = {.command = CMD_REP_SETTINGS, .seq_num = 0};
                            tx_pkt.payload.settings.kp = current_settings.kp;
                            tx_pkt.payload.settings.ki = current_settings.ki;
                            tx_pkt.payload.settings.kd = current_settings.kd;
                            tx_pkt.payload.settings.company_number = current_settings.company_number;
                            tx_pkt.payload.settings.profile_duration_s = current_settings.profile_duration_s;
                            currentlyTransmitting = true;
                            RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                        }
                    }
                    else if (state == FLOAT_PROFILE_DONE)
                    {
                        if (rx_pkt.command == CMD_SEND_DATA)
                        {
                            printf(">> Received SEND_DATA command. Starting data dump for scoring...\n");
                            state = FLOAT_DUMPING_DATA;
                            currentSeqNum = 1;
                            lastTxTime = 0;
                        }
                    }
                    else if (state == FLOAT_DUMPING_DATA)
                    {
                        if (rx_pkt.command == CMD_ACK && rx_pkt.seq_num == currentSeqNum)
                        {
                            printf(">> Received ACK for packet %d.\n", currentSeqNum);
                            currentSeqNum++;

                            if (currentSeqNum > sampleIndex) 
                            {
                                printf(">> All data sent. Sending DATA_DONE...\n");
                                packet_t done_pkt = {.command = CMD_DATA_DONE, .seq_num = 0};
                                currentlyTransmitting = true;
                                RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&done_pkt, sizeof(packet_t));
                                state = FLOAT_IDLE;
                            }
                            else
                            {
                                lastTxTime = 0;
                            }
                        }
                    }
                }
                if (!currentlyTransmitting)
                {
                    RadioLib_SX127x_StartReceive(&lora);
                }
            }
        }
        sleep_ms(1);
    }
    return 0;
}