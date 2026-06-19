#include "reflash_target.h"
#include "reflash_protocol.h"
#include "crc32.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "hardware/structs/xip_ctrl.h"
#include <stdio.h>
#include <string.h>

#define OTA_TIMEOUT_MS 30000  // Reset OTA state after 30s of no packets (outlasts 10 retries × 2s on surface)

static bool in_progress = false;
static uint32_t total_size = 0;
static uint32_t expected_master_crc = 0;
static uint32_t bytes_received = 0;
static uint32_t next_seq = 0;
static uint8_t page_buffer[512] __attribute__((aligned(4)));
static uint32_t page_buffer_idx = 0;
static uint32_t current_flash_addr = SLOT_1_OFFSET;
static uint32_t last_packet_ms = 0;  // Timestamp of last received OTA packet

// Critical Swap Routine: Must be in RAM, disable interrupts, erase Slot 0, copy Slot 1 -> Slot 0 in chunks
static void __no_inline_not_in_flash_func(critical_swap_routine)() {
    // 1. Disable all interrupts and watchdog
    uint32_t ints = save_and_disable_interrupts();
    (void)ints; // Suppress unused warning, we are rebooting anyway
    
    // Disable watchdog so it doesn't reboot us during the long copy process
    hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);

    // 2. Incremental Erase and Copy
    // We MUST read from Slot 1 (XIP) into RAM *before* we start an erase/program cycle for that sector.
    static uint8_t sector_buffer[FLASH_SECTOR_SIZE]; // 4KB RAM buffer
    
    for (uint32_t offset = 0; offset < total_size; offset += FLASH_SECTOR_SIZE) {
        // A. Read 4KB from Slot 1 (Flash) into RAM while XIP is still active/safe
        const uint8_t *src_ptr = (const uint8_t *)(XIP_BASE + SLOT_1_OFFSET + offset);
        memcpy(sector_buffer, src_ptr, FLASH_SECTOR_SIZE);

        // B. Erase 4KB sector in Slot 0 (Flash)
        flash_range_erase(SLOT_0_OFFSET + offset, FLASH_SECTOR_SIZE);

        // C. Program 4KB from RAM into Slot 0 (Flash)
        flash_range_program(SLOT_0_OFFSET + offset, sector_buffer, FLASH_SECTOR_SIZE);
    }

    // 3. Force a hardware reboot to load the new Slot 0
    // We use a 100ms delay to ensure the hardware has time to latch the reset
    watchdog_reboot(0, 0, 100);
    
    // Loop forever until the watchdog kicks in
    while (1) {
        tight_loop_contents();
    }
}

static void send_ack(RadioLibSX127x_t *lora, uint32_t seq) {
    // Disable rising-edge interrupt on the radio IRQ pin during synchronous transmit
    // to prevent the TxDone event from triggering a duplicate event check.
    if (lora->mod->irqPin != RADIOLIB_NC) {
        gpio_set_irq_enabled(lora->mod->irqPin, GPIO_IRQ_EDGE_RISE, false);
    }

    reflash_ack_msg_t ack = {.type = REFLASH_MSG_ACK, .seq_num = seq};
    RadioLib_SX127x_Transmit(lora, (uint8_t *)&ack, sizeof(ack));

    if (lora->mod->irqPin != RADIOLIB_NC) {
        // Acknowledge/clear the latch of any TxDone edge that occurred during transmission
        gpio_acknowledge_irq(lora->mod->irqPin, GPIO_IRQ_EDGE_RISE);
        // Re-enable interrupt
        gpio_set_irq_enabled(lora->mod->irqPin, GPIO_IRQ_EDGE_RISE, true);
    }
}

bool reflash_target_process_packet(RadioLibSX127x_t *lora, uint8_t *packet, size_t len) {
    if (len == 0) return false;
    
    uint8_t msg_type = packet[0];
    
    if (msg_type == REFLASH_MSG_START && len >= sizeof(reflash_start_msg_t)) {
        reflash_start_msg_t *start = (reflash_start_msg_t *)packet;
        if (start->total_size > SLOT_SIZE) {
            printf("[OTA] ERROR: Start Msg size %lu exceeds slot size %lu!\n", start->total_size, (uint32_t)SLOT_SIZE);
            return false;
        }
        total_size = start->total_size;
        expected_master_crc = start->master_crc;
        bytes_received = 0;
        next_seq = 0;
        
        printf("[OTA] Start Msg: Size %lu, CRC 0x%08lX. Erasing Slot 1...\n", total_size, expected_master_crc);
        
        // Send ACK at 125kHz so surface knows we are starting
        send_ack(lora, 0xFFFFFFFF);
        page_buffer_idx = 0;
        current_flash_addr = SLOT_1_OFFSET;
        
        // 3. Erase only what is needed for this binary (rounded up to 4KB sectors)
        uint32_t erase_len = (total_size + (FLASH_SECTOR_SIZE - 1)) & ~(FLASH_SECTOR_SIZE - 1);
        printf("[OTA] Erasing Slot 1 (%lu bytes)...\n", erase_len);

        for (uint32_t offset = 0; offset < erase_len; offset += FLASH_SECTOR_SIZE) {
            uint32_t ints = save_and_disable_interrupts();
            flash_range_erase(SLOT_1_OFFSET + offset, FLASH_SECTOR_SIZE);
            restore_interrupts(ints);
            watchdog_update(); // Keep watchdog alive during erase
        }
        
        printf("[OTA] Erase Complete. Waiting for data...\n");
        in_progress = true;
        last_packet_ms = to_ms_since_boot(get_absolute_time());
        return true;
    } 
    else if (msg_type == REFLASH_MSG_DATA && in_progress && len >= sizeof(reflash_data_msg_t)) {
        reflash_data_msg_t *data_pkt = (reflash_data_msg_t *)packet;
        if (data_pkt->seq_num == next_seq) {
            uint32_t pkt_crc = crc32_hardware(data_pkt->data, REFLASH_CHUNK_SIZE, 0xFFFFFFFF);
            if (pkt_crc == data_pkt->crc) {
                // Buffer the incoming chunk (Chunk size is 220)
                memcpy(&page_buffer[page_buffer_idx], data_pkt->data, REFLASH_CHUNK_SIZE);
                page_buffer_idx += REFLASH_CHUNK_SIZE;

                // Write as many full 256-byte pages as we have collected
                while (page_buffer_idx >= 256) {
                    uint32_t ints = save_and_disable_interrupts();
                    flash_range_program(current_flash_addr, page_buffer, 256);
                    restore_interrupts(ints);
                    
                    current_flash_addr += 256;
                    page_buffer_idx -= 256;
                    
                    // Shift leftovers to the front of the buffer
                    if (page_buffer_idx > 0) {
                        memmove(page_buffer, &page_buffer[256], page_buffer_idx);
                    }
                }
                
                send_ack(lora, next_seq);
                next_seq++;
                bytes_received += REFLASH_CHUNK_SIZE;
                last_packet_ms = to_ms_since_boot(get_absolute_time());
                printf("[OTA] Received Seq %lu (%lu/%lu)\n", data_pkt->seq_num, bytes_received, total_size);

                if (bytes_received >= total_size) {
                    printf("[OTA] All data received. Verifying master CRC...\n");
                    // Ensure any remaining buffered data is flushed (should be padded to 220 by Host)
                    if (page_buffer_idx > 0) {
                        memset(&page_buffer[page_buffer_idx], 0xFF, 256 - page_buffer_idx);
                        uint32_t ints = save_and_disable_interrupts();
                        flash_range_program(current_flash_addr, page_buffer, 256);
                        restore_interrupts(ints);
                        page_buffer_idx = 0;
                    }

                    // Flush the XIP cache robustly
                    xip_ctrl_hw->flush = 1;
                    while (!(xip_ctrl_hw->stat & XIP_STAT_FLUSH_RDY)) {
                        tight_loop_contents();
                    }

                    // Diagnostic Dumps
                    const uint8_t *slot1_ptr = (const uint8_t *)(XIP_BASE + SLOT_1_OFFSET);
                    printf("[OTA] First 16: ");
                    for(int i=0; i<16; i++) printf("%02x ", slot1_ptr[i]);
                    printf("\n[OTA] Mid (40k): ");
                    for(int i=0; i<16; i++) printf("%02x ", slot1_ptr[40000 + i]);
                    printf("\n[OTA] Last 16: ");
                    for(int i=0; i<16; i++) printf("%02x ", slot1_ptr[total_size - 16 + i]);
                    printf("\n");

                    // Use software CRC to match Python standard exactly
                    uint32_t actual_crc = crc32_software(slot1_ptr, total_size, 0xFFFFFFFF);
                    // Standard CRC32 applies a final XOR with 0xFFFFFFFF
                    actual_crc ^= 0xFFFFFFFF;

                    if (actual_crc == expected_master_crc) {
                        printf("[OTA] CRC Match! REBOOTING...\n");
                        sleep_ms(200);
                        critical_swap_routine();
                    } else {
                        printf("[OTA] CRC MISMATCH! Expected 0x%08lX, got 0x%08lX\n", expected_master_crc, actual_crc);
                        in_progress = false;
                    }
                }
            } else {
                printf("[OTA] Packet CRC Mismatch at Seq %lu\n", data_pkt->seq_num);
            }
        } else if (data_pkt->seq_num < next_seq) {
            // Old packet, just ACK again
            send_ack(lora, data_pkt->seq_num);
        }
        return true;
    }
    
    return false;
}

void reflash_target_tick(RadioLibSX127x_t *lora) {
    if (!in_progress) return;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_packet_ms > OTA_TIMEOUT_MS) {
        printf("[OTA] Transfer timeout! Resetting OTA state. Ready for new attempt.\n");
        in_progress = false;
        bytes_received = 0;
        next_seq = 0;
        page_buffer_idx = 0;
        current_flash_addr = SLOT_1_OFFSET;
        // Already at 125kHz — no BW reset needed
    }
}

bool reflash_target_is_in_progress(void) {
    return in_progress;
}