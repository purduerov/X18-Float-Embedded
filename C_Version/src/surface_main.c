#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- Shared Library Includes ---
#include "packets.h"
#include "radio_setup.h"

typedef enum
{
    SURFACE_IDLE,
    SURFACE_WAITING_PROFILE,
    SURFACE_DOWNLOADING
} SurfaceState_t;

const char *SurfaceStateNames[] = {
    "IDLE", "WAITING_PROFILE", "DOWNLOADING"};

// Structure to hold the downloaded MATE telemetry data in RAM
typedef struct
{
    uint16_t company_number;
    uint32_t time_ms;
    float depth_m;
} SensorReading_t;

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

    printf("\n\n=== MATE Surface Station Booting ===\n");

    // --- Initialize Radio (Using Shared Library) ---
    if (!radio_setup_init(onInterrupt))
    {
        printf("Radio init failed! Halting.\n");
        while (true)
            sleep_ms(1000);
    }

    printf("Surface Station Ready.\n");
    printf("Commands: 'p' (Profile), 's <P> <I> <D>' (PID), 'c <ID>' (Company ID), '?' (Sync Settings)\n");

    SurfaceState_t fsm_state = SURFACE_IDLE;
    bool currentlyTransmitting = false;
    uint8_t buffer[256];
    uint16_t expectedSeqNum = 1;

    // Dynamic Storage Variables
    SensorReading_t *downloaded_data = NULL;
    size_t downloaded_count = 0;
    size_t allocated_capacity = 0;

    uint32_t lastDebugPrint = to_ms_since_boot(get_absolute_time());

    RadioLib_SX127x_StartReceive(&lora);

    while (true)
    {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (now - lastDebugPrint >= 2000)
        {
            printf("[DEBUG] State: %s | Transmitting: %d | IRQ Flag: %d\n",
                   SurfaceStateNames[fsm_state], currentlyTransmitting, operationDoneFlag);
            lastDebugPrint = now;
        }

        static char input_line[64];
        static int input_pos = 0;

        int c;
        while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT)
        {
            if (c == '\n' || c == '\r')
            {
                if (input_pos > 0)
                {
                    input_line[input_pos] = '\0';

                    printf("\n[SERIAL] Received: %s\n", input_line);

                    if (fsm_state == SURFACE_IDLE && !currentlyTransmitting)
                    {
                        if (input_line[0] == 'p' || input_line[0] == 'P')
                        {
                            printf(">> Commanding BEGIN_PROFILE...\n");
                            packet_t tx_pkt = {.command = CMD_BEGIN_PROFILE, .seq_num = 0};
                            currentlyTransmitting = true;
                            RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                            fsm_state = SURFACE_WAITING_PROFILE;
                        }
                        else if (input_line[0] == 's' || input_line[0] == 'S')
                        {
                            float p, i, d;
                            if (sscanf(input_line + 1, "%f %f %f", &p, &i, &d) == 3)
                            {
                                printf(">> Sending PID: P=%.2f, I=%.2f, D=%.2f\n", p, i, d);
                                packet_t tx_pkt = {.command = CMD_SET_PID, .seq_num = 0};

                                tx_pkt.payload.settings.kp = p;
                                tx_pkt.payload.settings.ki = i;
                                tx_pkt.payload.settings.kd = d;

                                currentlyTransmitting = true;
                                RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                            }
                        }
                        else if (input_line[0] == 'c' || input_line[0] == 'C')
                        {
                            uint32_t parsed_id;
                            if (sscanf(input_line + 1, "%lu", &parsed_id) == 1)
                            {
                                printf(">> Sending Company ID Update: %lu\n", parsed_id);
                                packet_t tx_pkt = {.command = CMD_SET_COMPANY, .seq_num = 0};
                                tx_pkt.payload.telemetry.company_number = (uint16_t)parsed_id;
                                currentlyTransmitting = true;
                                RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                            }
                        }
                        else if (input_line[0] == 't' || input_line[0] == 'T')
                        {
                            uint32_t parsed_time;
                            if (sscanf(input_line + 1, "%lu", &parsed_time) == 1)
                            {
                                printf(">> Sending Duration Update: %lu sec\n", parsed_time);
                                packet_t tx_pkt = {.command = CMD_SET_DURATION, .seq_num = 0};
                                tx_pkt.payload.settings.profile_duration_s = (uint16_t)parsed_time;
                                currentlyTransmitting = true;
                                RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                            }
                        }
                        else if (input_line[0] == '?')
                        {
                            printf(">> Requesting current float settings...\n");
                            packet_t tx_pkt = {.command = CMD_REQ_SETTINGS, .seq_num = 0};
                            currentlyTransmitting = true;
                            RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                        }
                    }
                    input_pos = 0;
                }
            }
            else if (c == '\b' || c == 127)
            {
                if (input_pos > 0)
                {
                    input_pos--;
                    printf("\b \b");
                }
            }
            else if (c >= 32 && c <= 126)
            {
                if (input_pos < sizeof(input_line) - 1)
                {
                    input_line[input_pos++] = (char)c;
                    putchar(c);
                }
            }
        }

        if (operationDoneFlag)
        {
            operationDoneFlag = false;

            if (currentlyTransmitting)
            {
                RadioLib_SX127x_FinishTransmit(&lora);
                currentlyTransmitting = false;
                RadioLib_SX127x_StartReceive(&lora);
            }
            else
            {
                int16_t len = RadioLib_SX127x_ReadData(&lora, buffer, sizeof(buffer));

                if (len == sizeof(packet_t))
                {
                    packet_t rx_pkt;
                    memcpy(&rx_pkt, buffer, sizeof(packet_t));

                    if (fsm_state == SURFACE_IDLE)
                    {
                        if (rx_pkt.command == CMD_REP_SETTINGS)
                        {
                            // This exact format is required for the Python split(",") logic
                            printf("\n[SYNC] FLOAT_SETTINGS: P=%.2f, I=%.2f, D=%.2f, Co#=%u, Time=%u\n",
                                   rx_pkt.payload.settings.kp,
                                   rx_pkt.payload.settings.ki,
                                   rx_pkt.payload.settings.kd,
                                   rx_pkt.payload.settings.company_number,
                                   rx_pkt.payload.settings.profile_duration_s);
                        }
                    }
                    else if (fsm_state == SURFACE_WAITING_PROFILE)
                    {
                        if (rx_pkt.command == CMD_DATA_TRANSMISSION && rx_pkt.seq_num == 0)
                        {
                            printf(">> PRE-DIVE Packet Logged: Co# %u | Time %lu ms | Depth %.2f m\n",
                                   rx_pkt.payload.telemetry.company_number,
                                   rx_pkt.payload.telemetry.time_ms,
                                   rx_pkt.payload.telemetry.depth_m);
                        }
                        else if (rx_pkt.command == CMD_DONE_PROFILE)
                        {
                            printf(">> Float finished profile! Sending SEND_DATA command...\n");

                            if (downloaded_data != NULL)
                            {
                                free(downloaded_data);
                            }
                            allocated_capacity = 16;
                            downloaded_count = 0;
                            downloaded_data = (SensorReading_t *)malloc(allocated_capacity * sizeof(SensorReading_t));

                            packet_t tx_pkt = {.command = CMD_SEND_DATA, .seq_num = 0};
                            currentlyTransmitting = true;
                            RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));

                            fsm_state = SURFACE_DOWNLOADING;
                            expectedSeqNum = 1;
                        }
                    }
                    else if (fsm_state == SURFACE_DOWNLOADING)
                    {
                        if (rx_pkt.command == CMD_DATA_TRANSMISSION)
                        {
                            if (rx_pkt.seq_num == expectedSeqNum)
                            {
                                if (downloaded_count >= allocated_capacity)
                                {
                                    allocated_capacity *= 2;
                                    SensorReading_t *temp = (SensorReading_t *)realloc(downloaded_data, allocated_capacity * sizeof(SensorReading_t));

                                    if (temp != NULL)
                                    {
                                        downloaded_data = temp;
                                    }
                                    else
                                    {
                                        printf(">> [ERROR] Memory allocation failed during realloc!\n");
                                    }
                                }

                                if (downloaded_data != NULL)
                                {
                                    downloaded_data[downloaded_count].company_number = rx_pkt.payload.telemetry.company_number;
                                    downloaded_data[downloaded_count].time_ms = rx_pkt.payload.telemetry.time_ms;
                                    downloaded_data[downloaded_count].depth_m = rx_pkt.payload.telemetry.depth_m;

                                    printf(">> Stored Data #%d: Co# %u | Time %lu ms | Depth %.2f m\n",
                                           rx_pkt.seq_num,
                                           downloaded_data[downloaded_count].company_number,
                                           downloaded_data[downloaded_count].time_ms,
                                           downloaded_data[downloaded_count].depth_m);
                                    downloaded_count++;
                                }

                                expectedSeqNum++;
                            }
                            else
                            {
                                printf(">> Received duplicate/old packet #%d. Re-sending ACK.\n", rx_pkt.seq_num);
                            }

                            packet_t ack_pkt = {.command = CMD_ACK, .seq_num = rx_pkt.seq_num};
                            currentlyTransmitting = true;
                            RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&ack_pkt, sizeof(packet_t));
                        }
                        else if (rx_pkt.command == CMD_DATA_DONE)
                        {
                            printf("\n--- START DATA DUMP ---\n");
                            printf("CompanyNumber,Time(ms),Depth(m)\n");

                            for (size_t i = 0; i < downloaded_count; i++)
                            {
                                printf("%u,%lu,%.2f\n",
                                       downloaded_data[i].company_number,
                                       downloaded_data[i].time_ms,
                                       downloaded_data[i].depth_m);
                            }

                            printf("--- END DATA DUMP ---\n");
                            printf(">> Download Complete! Total Packets Received: %u\n", downloaded_count);

                            fsm_state = SURFACE_IDLE;
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