#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "radiolib_hal_pico.h"
#include "radiolib_sx1276.h"
#include "reflash_protocol.h"
#include "crc32.h"

// --- Radio Configuration ---
const uint32_t SPI_MOSI = 19;
const uint32_t SPI_MISO = 20;
const uint32_t SPI_SCK = 18;
const uint32_t CS_PIN = 24;
const uint32_t RST_PIN = 25;
const uint32_t EN_PIN = 8;
const uint32_t IRQ_PIN = 9;

static RadioLibSX127x_t lora;

void init_radio() {
    gpio_init(EN_PIN);
    gpio_set_dir(EN_PIN, GPIO_OUT);
    gpio_put(EN_PIN, 1);
    sleep_ms(10);

    RadioLibHal_t *hal = RadioLib_Pico_Create(spi0, SPI_SCK, SPI_MOSI, SPI_MISO, 8000000);
    RadioLibModule_t radioModule;
    RadioLib_Module_Create(&radioModule, hal, CS_PIN, IRQ_PIN, RST_PIN, RADIOLIB_NC);
    radioModule.enPin = EN_PIN;
    
    uint32_t gPins[] = {RADIOLIB_NC, 29, 6, 7, 10, 11};
    for (int i = 0; i < 6; i++) radioModule.radioGPins[i] = gPins[i];

    RadioLib_SX127x_Create(&lora, &radioModule);
    
    // SF7, 500kHz BW, CR 4/5 as per requirement
    int16_t state = RadioLib_SX1276_Begin(&lora, 915.0, 500.0, 7, 5, 10, 8);
    if (state != RADIOLIB_ERR_NONE) {
        printf("Radio init failed, code %d\n", state);
        while (1) tight_loop_contents();
    }
    printf("Radio initialized: SF7, 500kHz BW\n");
}

bool wait_for_ack(uint32_t expected_seq) {
    uint8_t buffer[256];
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start_time < 2000) {
        int16_t state = RadioLib_SX127x_Receive(&lora, buffer, sizeof(buffer));
        if (state == RADIOLIB_ERR_NONE) {
            reflash_ack_msg_t *ack = (reflash_ack_msg_t *)buffer;
            if (ack->type == REFLASH_MSG_ACK && ack->seq_num == expected_seq) {
                return true;
            }
        }
        tight_loop_contents();
    }
    return false;
}

int main() {
    stdio_init_all();
    init_radio();

    while (1) {
        printf("Waiting for PC to send reflash command (format: S[size:4][crc:4])...\n");
        int c = getchar();
        if (c == 'S') {
            uint32_t total_size = 0;
            uint32_t master_crc = 0;
            for (int i = 0; i < 4; i++) total_size |= ((uint32_t)getchar() << (i * 8));
            for (int i = 0; i < 4; i++) master_crc |= ((uint32_t)getchar() << (i * 8));

            printf("Starting reflash: Size %u, CRC 0x%08X\n", total_size, master_crc);

            reflash_start_msg_t start_msg = {
                .type = REFLASH_MSG_START,
                .total_size = total_size,
                .master_crc = master_crc
            };

            bool started = false;
            for (int retry = 0; retry < 5; retry++) {
                RadioLib_SX127x_Transmit(&lora, (uint8_t *)&start_msg, sizeof(start_msg));
                if (wait_for_ack(0xFFFFFFFF)) { // Using 0xFFFFFFFF as seq for start
                    started = true;
                    break;
                }
                printf("Start retry %d...\n", retry + 1);
            }

            if (!started) {
                printf("Failed to start reflash (no ACK from receiver)\n");
                continue;
            }

            uint32_t bytes_sent = 0;
            uint32_t seq_num = 0;
            reflash_data_msg_t data_pkt;
            data_pkt.type = REFLASH_MSG_DATA;

            while (bytes_sent < total_size) {
                uint32_t chunk_len = (total_size - bytes_sent > REFLASH_CHUNK_SIZE) ? REFLASH_CHUNK_SIZE : (total_size - bytes_sent);
                for (uint32_t i = 0; i < chunk_len; i++) {
                    data_pkt.data[i] = getchar();
                }
                // Fill rest with 0 if needed (though not strictly necessary for flash)
                if (chunk_len < REFLASH_CHUNK_SIZE) {
                    memset(data_pkt.data + chunk_len, 0xFF, REFLASH_CHUNK_SIZE - chunk_len);
                }

                data_pkt.seq_num = seq_num;
                data_pkt.crc = crc32_hardware(data_pkt.data, REFLASH_CHUNK_SIZE, 0xFFFFFFFF);

                bool pkt_acked = false;
                for (int retry = 0; retry < 10; retry++) {
                    RadioLib_SX127x_Transmit(&lora, (uint8_t *)&data_pkt, sizeof(data_pkt));
                    if (wait_for_ack(seq_num)) {
                        pkt_acked = true;
                        break;
                    }
                    printf("Seq %u retry %d...\n", seq_num, retry + 1);
                }

                if (!pkt_acked) {
                    printf("Link lost at seq %u\n", seq_num);
                    break;
                }

                bytes_sent += chunk_len;
                seq_num++;
                printf("Progress: %u/%u\n", bytes_sent, total_size);
            }

            if (bytes_sent == total_size) {
                printf("Reflash transfer complete!\n");
            }
        }
    }

    return 0;
}
