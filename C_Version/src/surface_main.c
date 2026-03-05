#include "pico/stdlib.h"
#include "radiolib_hal_pico.h"
#include "radiolib_sx1276.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Required for malloc, realloc, free

const uint32_t SPI_MOSI = 19;
const uint32_t SPI_MISO = 20;
const uint32_t SPI_SCK = 18;
const uint32_t CS_PIN = 24;
const uint32_t RST_PIN = 25;
const uint32_t EN_PIN = 8;
const uint32_t IRQ_PIN = 9;

typedef enum
{
    CMD_NONE = 0x00,
    CMD_SEND_DATA = 0x01,
    CMD_DATA_TRANSMISSION = 0x02,
    CMD_SET_PID = 0x03,
    CMD_BEGIN_PROFILE = 0x04,
    CMD_DONE_PROFILE = 0x05,
    CMD_DATA_DONE = 0x06,
    CMD_ACK = 0x07
} PacketCommand_t;

typedef struct __attribute__((packed))
{
    uint8_t command;
    uint16_t seq_num;
    uint8_t payload[12];
} packet_t;

typedef enum
{
    SURFACE_IDLE,
    SURFACE_WAITING_PROFILE,
    SURFACE_DOWNLOADING
} SurfaceState_t;

const char *SurfaceStateNames[] = {
    "IDLE", "WAITING_PROFILE", "DOWNLOADING"};

// Structure to hold a single downloaded reading in RAM
typedef struct {
    float values[3]; 
} SensorReading_t;

volatile bool operationDoneFlag = false;
void onInterrupt(void) { operationDoneFlag = true; }

int main()
{
    stdio_init_all();
    gpio_init(EN_PIN);
    gpio_set_dir(EN_PIN, GPIO_OUT);
    gpio_put(EN_PIN, 1);
    sleep_ms(100);

    uint32_t waitTime = 0;
    while (!stdio_usb_connected() && waitTime < 5000)
    {
        sleep_ms(100);
        waitTime += 100;
    }

    printf("\n\n=== Surface Station Booting ===\n");

    RadioLibHal_t *hal = RadioLib_Pico_Create(spi0, SPI_SCK, SPI_MOSI, SPI_MISO, 8000000);
    RadioLibModule_t radioModule;
    RadioLib_Module_Create(&radioModule, hal, CS_PIN, IRQ_PIN, RST_PIN, RADIOLIB_NC);
    radioModule.enPin = EN_PIN;

    uint32_t gPins[] = {RADIOLIB_NC, 29, 6, 7, 10, 11};
    for (int i = 0; i < 6; i++)
        radioModule.radioGPins[i] = gPins[i];

    RadioLibSX127x_t lora;
    RadioLib_SX127x_Create(&lora, &radioModule);

    printf("Initializing SX1276...\n");
    int16_t radio_status = RadioLib_SX1276_Begin(&lora, 915.0, 125.0, 7, 10);
    if (radio_status != RADIOLIB_ERR_NONE)
    {
        printf("CRITICAL ERROR: Radio Init failed, code %d\n", radio_status);
        printf("Check your wiring! Freezing program here.\n");
        while (true)
            sleep_ms(1000);
    }
    printf("Radio Init Success!\n");

    RadioLib_SX127x_SetAction(&lora, onInterrupt);

    printf("Surface Station Ready.\n");
    printf("Type 'p' to begin profile. Type 's' to set test PID.\n");

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
                                float new_gains[3] = {p, i, d};
                                memcpy(tx_pkt.payload, new_gains, 12);
                                currentlyTransmitting = true;
                                RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                            }
                            else
                            {
                                printf(">> [ERROR] Invalid PID format. Please use format: s <P> <I> <D>\n");
                            }
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

                    if (fsm_state == SURFACE_WAITING_PROFILE)
                    {
                        if (rx_pkt.command == CMD_DONE_PROFILE)
                        {
                            printf(">> Float finished profile! Sending SEND_DATA command...\n");
                            
                            // Initialize dynamic array for the incoming data dump
                            if (downloaded_data != NULL) {
                                free(downloaded_data); // Clear any old data from a previous dive
                            }
                            allocated_capacity = 16; // Start by allocating space for 16 packets
                            downloaded_count = 0;
                            downloaded_data = (SensorReading_t*)malloc(allocated_capacity * sizeof(SensorReading_t));

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
                                // Check if we need to expand the array
                                if (downloaded_count >= allocated_capacity) {
                                    allocated_capacity *= 2; // Double the capacity
                                    SensorReading_t *temp = (SensorReading_t*)realloc(downloaded_data, allocated_capacity * sizeof(SensorReading_t));
                                    
                                    if (temp != NULL) {
                                        downloaded_data = temp;
                                    } else {
                                        printf(">> [ERROR] Memory allocation failed during realloc!\n");
                                    }
                                }

                                // Store the payload securely into our dynamic array
                                if (downloaded_data != NULL) {
                                    memcpy(downloaded_data[downloaded_count].values, rx_pkt.payload, 12);
                                    printf(">> Stored Data #%d: [%.2f, %.2f, %.2f]\n", 
                                        rx_pkt.seq_num, 
                                        downloaded_data[downloaded_count].values[0], 
                                        downloaded_data[downloaded_count].values[1], 
                                        downloaded_data[downloaded_count].values[2]);
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
                            printf(">> Download Complete! Total Packets Stored: %u\n", downloaded_count);
                            
                            // Print out all stored data to verify it is held in RAM correctly
                            printf("\n=== STORED PROFILE DATA ===\n");
                            for (size_t i = 0; i < downloaded_count; i++) {
                                printf("Packet %u: Depth = %.2f m\n", i + 1, downloaded_data[i].values[0]);
                            }
                            printf("===========================\n\n");
                            
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