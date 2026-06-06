#include "reflash_host.h"
#include "reflash_protocol.h"
#include "crc32.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#include "hardware/watchdog.h"
#include "hw_config.h"

static bool wait_for_ack(RadioLibSX127x_t *lora, uint32_t expected_seq) {
    uint8_t buffer[256];
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    
    // Put radio in receive mode
    RadioLib_SX127x_StartReceive(lora);

    while (to_ms_since_boot(get_absolute_time()) - start_time < 2000) {
        // Poll the IRQ pin (target-specific via hw_config.h)
        if (gpio_get(PIN_IRQ)) {
            int16_t state = RadioLib_SX127x_ReadData(lora, buffer, sizeof(buffer));
            if (state > 0 && buffer[0] == REFLASH_MSG_ACK) {
                reflash_ack_msg_t *ack = (reflash_ack_msg_t *)buffer;
                if (ack->seq_num == expected_seq) {
                    printf("[HOST] ACK received for seq %lu\n", ack->seq_num);
                    return true;
                }
            }
            // Resume listening if it wasn't the ACK we wanted
            RadioLib_SX127x_StartReceive(lora);
        }
        tight_loop_contents();
    }
    return false;
}

void reflash_host_stream_from_serial(RadioLibSX127x_t *lora) {
    uint32_t total_size = 0;
    uint32_t master_crc = 0;
    
    // Read 8 bytes (4 size, 4 crc). 'S' is already consumed by surface_main.c
    uint8_t header[8];
    for (int i = 0; i < 8; i++) {
        int c = getchar_timeout_us(5000000);
        if (c == PICO_ERROR_TIMEOUT) {
            printf("[HOST] Timeout reading header byte %d. Aborting.\n", i);
            RadioLib_SX127x_SetBandwidth(lora, REFLASH_BW_NORMAL);
            return;
        }
        header[i] = (uint8_t)c;
    }

    // Unpack Little-Endian
    total_size = header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
    master_crc = header[4] | (header[5] << 8) | (header[6] << 16) | (header[7] << 24);

    printf("[HOST] Handshake: Size %lu, CRC 0x%08lX\n", total_size, master_crc);
    printf("[HOST] Syncing with Float over Radio...\n");

    reflash_start_msg_t start_msg = {
        .type = REFLASH_MSG_START,
        .total_size = total_size,
        .master_crc = master_crc
    };

    bool started = false;
    for (int retry = 0; retry < 5; retry++) {
        // Transmit START at 125kHz
        RadioLib_SX127x_Transmit(lora, (uint8_t *)&start_msg, sizeof(start_msg));

        if (wait_for_ack(lora, 0xFFFFFFFF)) {
            started = true;
            // Switch to high-speed reflash bandwidth
            RadioLib_SX127x_SetBandwidth(lora, REFLASH_BW_FAST);
            break;
        }
        printf("[HOST] Start retry %d...\n", retry + 1);
    }

    if (!started) {
        printf("[HOST] Failed to start reflash (no ACK from receiver)\n");
        RadioLib_SX127x_SetBandwidth(lora, REFLASH_BW_NORMAL); // Reset bandwidth on failure
        return; // Return to normal operations
    }

    uint32_t bytes_sent = 0;
    uint32_t seq_num = 0;
    reflash_data_msg_t data_pkt;
    data_pkt.type = REFLASH_MSG_DATA;

    while (bytes_sent < total_size) {
        uint32_t chunk_len = (total_size - bytes_sent > REFLASH_CHUNK_SIZE) ? REFLASH_CHUNK_SIZE : (total_size - bytes_sent);
        for (uint32_t i = 0; i < chunk_len; i++) {
            int c = getchar_timeout_us(5000000); // 5s per byte — covers LoRa retry window
            if (c == PICO_ERROR_TIMEOUT) {
                printf("[HOST] Stalled waiting for data byte %lu of seq %lu. Aborting.\n", i, seq_num);
                RadioLib_SX127x_SetBandwidth(lora, REFLASH_BW_NORMAL); // restore normal BW
                return; // back to main loop — surface will resume printing
            }
            data_pkt.data[i] = (uint8_t)c;
        }
        if (chunk_len < REFLASH_CHUNK_SIZE) {
            memset(data_pkt.data + chunk_len, 0xFF, REFLASH_CHUNK_SIZE - chunk_len);
        }

        data_pkt.seq_num = seq_num;
        data_pkt.crc = crc32_hardware(data_pkt.data, REFLASH_CHUNK_SIZE, 0xFFFFFFFF);

        bool pkt_acked = false;
        for (int retry = 0; retry < 10; retry++) {
            RadioLib_SX127x_Transmit(lora, (uint8_t *)&data_pkt, sizeof(data_pkt));
            if (wait_for_ack(lora, seq_num)) {
                pkt_acked = true;
                break;
            }
            printf("[HOST] Seq %lu retry %d...\n", seq_num, retry + 1);
        }

        if (!pkt_acked) {
            printf("[HOST] Link lost at seq %lu\n", seq_num);
            RadioLib_SX127x_SetBandwidth(lora, REFLASH_BW_NORMAL); // restore normal BW
            break;
        }

        bytes_sent += chunk_len;
        seq_num++;
        // Output progress for flash_tool.py to consume (or just for humans)
        printf("Progress: %lu/%lu\n", bytes_sent, total_size);
    }

    if (bytes_sent == total_size) {
        printf("[HOST] Reflash transfer complete! Rebooting Surface...\n");
        RadioLib_SX127x_SetBandwidth(lora, REFLASH_BW_NORMAL); // restore normal BW
        sleep_ms(500);
        watchdog_reboot(0, 0, 100);
    }
}