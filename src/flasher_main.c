#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
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
    
    int16_t state = RadioLib_SX1276_Begin(&lora, 915.0, 500.0, 7, 5, 10, 8, 0x12);
    if (state != RADIOLIB_ERR_NONE) {
        printf("Radio init failed\n");
        while (1);
    }
}

// Critical Swap Routine: Must be in RAM, disable interrupts, erase Slot 0, copy Slot 1 -> Slot 0 in chunks
void __no_inline_not_in_flash_func(critical_swap_routine)() {
    // 1. Disable all interrupts
    uint32_t ints = save_and_disable_interrupts();

    // 2. Erase Slot 0 (1MB)
    // Erase in 4KB sectors (FLASH_SECTOR_SIZE)
    for (uint32_t offset = 0; offset < SLOT_SIZE; offset += FLASH_SECTOR_SIZE) {
        flash_range_erase(SLOT_0_OFFSET + offset, FLASH_SECTOR_SIZE);
    }

    // 3. Copy Slot 1 to Slot 0 in chunks using a RAM buffer
    // We'll use a 4KB buffer in RAM
    static uint8_t ram_buffer[4096];
    for (uint32_t offset = 0; offset < SLOT_SIZE; offset += sizeof(ram_buffer)) {
        // A. Read from Slot 1 (XIP access is OK here because we are not programming yet)
        const uint8_t *slot1_ptr = (const uint8_t *)(XIP_BASE + SLOT_1_OFFSET + offset);
        memcpy(ram_buffer, slot1_ptr, sizeof(ram_buffer));

        // B. Write to Slot 0 (No XIP access during this call)
        flash_range_program(SLOT_0_OFFSET + offset, ram_buffer, sizeof(ram_buffer));
    }

    // 4. Reboot
    watchdog_reboot(0, 0, 0);
    while (1) tight_loop_contents();
}

void send_ack(uint32_t seq) {
    reflash_ack_msg_t ack = {.type = REFLASH_MSG_ACK, .seq_num = seq};
    RadioLib_SX127x_Transmit(&lora, (uint8_t *)&ack, sizeof(ack));
}

int main() {
    stdio_init_all();
    init_radio();
    watchdog_enable(5000, 1);

    uint8_t rx_buffer[512];
    uint8_t page_buffer[512];
    uint32_t page_buffer_idx = 0;
    uint32_t current_flash_addr = SLOT_1_OFFSET;
    uint32_t total_size = 0;
    uint32_t expected_master_crc = 0;
    uint32_t bytes_received = 0;
    uint32_t next_seq = 0;
    bool in_progress = false;

    printf("Flasher Ready. Waiting for REFLASH_START...\n");

    while (1) {
        watchdog_update();
        int16_t state = RadioLib_SX127x_Receive(&lora, rx_buffer, sizeof(rx_buffer));
        
        if (state == RADIOLIB_ERR_NONE) {
            uint8_t msg_type = rx_buffer[0];
            
            if (msg_type == REFLASH_MSG_START) {
                reflash_start_msg_t *start = (reflash_start_msg_t *)rx_buffer;
                if (start->total_size > SLOT_SIZE) {
                    printf("ERROR: Start Msg size %lu exceeds slot size %lu!\n", start->total_size, (uint32_t)SLOT_SIZE);
                    continue;
                }
                total_size = start->total_size;
                expected_master_crc = start->master_crc;
                bytes_received = 0;
                next_seq = 0;
                page_buffer_idx = 0;
                current_flash_addr = SLOT_1_OFFSET;
                
                printf("Start Msg: Size %u, CRC 0x%08X. Erasing Slot 1...\n", total_size, expected_master_crc);
                flash_range_erase(SLOT_1_OFFSET, SLOT_SIZE);
                
                send_ack(0xFFFFFFFF);
                in_progress = true;
            } 
            else if (msg_type == REFLASH_MSG_DATA && in_progress) {
                reflash_data_msg_t *data_pkt = (reflash_data_msg_t *)rx_buffer;
                if (data_pkt->seq_num == next_seq) {
                    // Verify packet CRC
                    uint32_t pkt_crc = crc32_hardware(data_pkt->data, REFLASH_CHUNK_SIZE, 0xFFFFFFFF);
                    if (pkt_crc == data_pkt->crc) {
                        // Buffer the incoming chunk (Chunk size is 220)
                        memcpy(&page_buffer[page_buffer_idx], data_pkt->data, REFLASH_CHUNK_SIZE);
                        page_buffer_idx += REFLASH_CHUNK_SIZE;

                        // Write as many full 256-byte pages as we have collected
                        while (page_buffer_idx >= 256) {
                            flash_range_program(current_flash_addr, page_buffer, 256);
                            current_flash_addr += 256;
                            page_buffer_idx -= 256;
                            
                            // Shift leftovers to the front
                            if (page_buffer_idx > 0) {
                                memmove(page_buffer, &page_buffer[256], page_buffer_idx);
                            }
                        }
                        
                        send_ack(next_seq);
                        next_seq++;
                        bytes_received += REFLASH_CHUNK_SIZE;
                        printf("Received Seq %u (%u/%u)\n", data_pkt->seq_num, bytes_received, total_size);

                        // Check if complete
                        if (bytes_received >= total_size) {
                            // Ensure any remaining buffered data is flushed (should be padded to 220 by Host)
                            if (page_buffer_idx > 0) {
                                // Pad the rest of the 256-byte page with 0xFF
                                memset(&page_buffer[page_buffer_idx], 0xFF, 256 - page_buffer_idx);
                                flash_range_program(current_flash_addr, page_buffer, 256);
                                current_flash_addr += 256;
                                page_buffer_idx = 0;
                            }

                            printf("All data received. Verifying master CRC...\n");
                            uint32_t actual_crc = crc32_hardware((const uint8_t *)(XIP_BASE + SLOT_1_OFFSET), total_size, 0xFFFFFFFF);
                            if (actual_crc == expected_master_crc) {
                                printf("CRC Match! Initiating Critical Swap...\n");
                                sleep_ms(100);
                                critical_swap_routine();
                            } else {
                                printf("CRC MISMATCH! Expected 0x%08X, got 0x%08X\n", expected_master_crc, actual_crc);
                                in_progress = false;
                            }
                        }
                    } else {
                        printf("Packet CRC Mismatch at Seq %u\n", data_pkt->seq_num);
                    }
                } else if (data_pkt->seq_num < next_seq) {
                    // Old packet, just ACK again
                    send_ack(data_pkt->seq_num);
                }
            }
        }
        tight_loop_contents();
    }

    return 0;
}
