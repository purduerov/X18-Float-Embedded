#include "pico/stdlib.h"
#include "radiolib_hal_pico.h"
#include "radiolib_sx1276.h"
#include "hardware/i2c.h"
#include "ms5837.h"
#include <stdio.h>
#include <string.h>

// --- Radio Setup ---
const uint32_t SPI_MOSI = 19;
const uint32_t SPI_MISO = 20;
const uint32_t SPI_SCK = 18;
const uint32_t CS_PIN = 24;
const uint32_t RST_PIN = 25;
const uint32_t EN_PIN = 8;
const uint32_t IRQ_PIN = 9;

// --- Profiling Configuration ---
#define PROFILE_DURATION_MS 180000 
#define SAMPLE_INTERVAL_MS  1000
#define TOTAL_PACKETS (PROFILE_DURATION_MS / SAMPLE_INTERVAL_MS)

float recorded_depths[TOTAL_PACKETS];
uint32_t recorded_times[TOTAL_PACKETS];
uint16_t active_company_number = 9999; // Default, can be updated via radio

// --- I2C / Sensor Setup ---
#define I2C_PORT i2c1
#define PIN_SDA 2
#define PIN_SCL 3
#define SENSOR_TOP_OFFSET 0.465f

typedef enum
{
    CMD_NONE = 0x00,
    CMD_SEND_DATA = 0x01,
    CMD_DATA_TRANSMISSION = 0x02,
    CMD_SET_PID = 0x03,
    CMD_BEGIN_PROFILE = 0x04,
    CMD_DONE_PROFILE = 0x05,
    CMD_DATA_DONE = 0x06,
    CMD_ACK = 0x07,
    CMD_PREDIVE_READY = 0x08,
    CMD_SET_COMPANY = 0x09
} PacketCommand_t;

typedef struct __attribute__((packed))
{
    uint8_t command;
    uint16_t seq_num;
    union {
        struct {
            uint16_t company_number;
            uint32_t time_ms;
            float depth_m;
        } telemetry;         
        float pid_gains[3];  
        uint8_t raw[12];     
    } payload;
} packet_t;

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

    printf("\n\n=== MATE Float Station Booting ===\n");

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

    // --- Initialize Radio ---
    RadioLibHal_t *hal = RadioLib_Pico_Create(spi0, SPI_SCK, SPI_MOSI, SPI_MISO, 8000000);
    RadioLibModule_t radioModule;
    memset(&radioModule, 0, sizeof(RadioLibModule_t)); 
    RadioLib_Module_Create(&radioModule, hal, CS_PIN, IRQ_PIN, RST_PIN, RADIOLIB_NC);
    radioModule.enPin = EN_PIN;

    uint32_t gPins[] = {RADIOLIB_NC, 29, 6, 7, 10, 11};
    for (int i = 0; i < 6; i++)
        radioModule.radioGPins[i] = gPins[i];

    RadioLibSX127x_t lora;
    RadioLib_SX127x_Create(&lora, &radioModule);
    RadioLib_SX1276_Begin(&lora, 915.0, 125.0, 7, 10);
    RadioLib_SX127x_SetAction(&lora, onInterrupt);

    printf("Float Initialized. Waiting in IDLE state...\n");

    FloatState_t state = FLOAT_IDLE;
    bool currentlyTransmitting = false;
    uint8_t buffer[256];

    // FSM Timing & Tracking
    uint32_t profileStartTime = 0;
    uint32_t lastTxTime = 0;
    uint32_t lastSampleTime = 0;

    // Data Storage for the Profile
    uint16_t currentSeqNum = 1;
    uint16_t sampleIndex = 0;

    float pid_gains[3] = {1.0f, 0.5f, 0.1f};
    uint32_t lastDebugPrint = to_ms_since_boot(get_absolute_time());

    RadioLib_SX127x_StartReceive(&lora);

    while (true)
    {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // --- DEBUG PRINTOUT ---
        if (now - lastDebugPrint >= 2000)
        {
            printf("[DEBUG] State: %s | Transmitting: %d | IRQ Flag: %d\n",
                   FloatStateNames[state], currentlyTransmitting, operationDoneFlag);
            lastDebugPrint = now;
        }

        // --- FSM TIMEOUTS & PERIODIC ACTIONS ---
        if (state == FLOAT_PRE_DIVE && !currentlyTransmitting)
        {
            printf(">> Sending Pre-Dive Data Packet...\n");
            
            packet_t tx_pkt = {.command = CMD_DATA_TRANSMISSION, .seq_num = 0};
            tx_pkt.payload.telemetry.company_number = active_company_number;
            tx_pkt.payload.telemetry.time_ms = now;
            tx_pkt.payload.telemetry.depth_m = 0.0f;

            currentlyTransmitting = true;
            RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
        }
        else if (state == FLOAT_PROFILING)
        {
            if (now - lastSampleTime >= SAMPLE_INTERVAL_MS && sampleIndex < TOTAL_PACKETS)
            {
                ms5837_read(&depth_sensor);
                recorded_depths[sampleIndex] = ms5837_get_depth(&depth_sensor) - SENSOR_TOP_OFFSET;
                recorded_times[sampleIndex] = now;

                printf(">> Sample %u/%lu: Time %lu ms | Depth %.2f m\n",
                       sampleIndex + 1, TOTAL_PACKETS, recorded_times[sampleIndex], recorded_depths[sampleIndex]);

                sampleIndex++;
                lastSampleTime = now;
            }

            if (now - profileStartTime >= PROFILE_DURATION_MS)
            {
                printf(">> Profile complete (%lu ms). Surfacing...\n", PROFILE_DURATION_MS);
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
                
                tx_pkt.payload.telemetry.company_number = active_company_number;
                tx_pkt.payload.telemetry.time_ms = recorded_times[currentSeqNum - 1];
                tx_pkt.payload.telemetry.depth_m = recorded_depths[currentSeqNum - 1];

                currentlyTransmitting = true;
                RadioLib_SX127x_StartTransmit(&lora, (uint8_t *)&tx_pkt, sizeof(packet_t));
                lastTxTime = now;
            }
        }

        // --- INTERRUPT HANDLING ---
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
                            memcpy(pid_gains, rx_pkt.payload.pid_gains, 12);
                            printf(">> PID Updated: P=%.2f, I=%.2f, D=%.2f\n", pid_gains[0], pid_gains[1], pid_gains[2]);
                        }
                        else if (rx_pkt.command == CMD_SET_COMPANY)
                        {
                            active_company_number = rx_pkt.payload.telemetry.company_number;
                            printf(">> Company ID Updated: %u\n", active_company_number);
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