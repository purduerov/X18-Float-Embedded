#include "crc32.h"
#include "hardware/dma.h"
#include <stdbool.h>

uint32_t crc32_hardware(const uint8_t *data, size_t len, uint32_t seed) {
    // 1. Claim a DMA channel
    int chan = dma_claim_unused_channel(true);

    // 2. Configure the channel for a memory-to-memory transfer
    dma_channel_config c = dma_channel_get_default_config(chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8); // 8-bit transfers
    channel_config_set_read_increment(&c, true);          // Read from buffer
    channel_config_set_write_increment(&c, false);         // Write to same location (dummy)
    channel_config_set_sniff_enable(&c, true);            // Enable sniffer

    // 3. Configure the sniffer
    // Mode 0: CRC-32 (IEEE 802.3 polynomial)
    dma_sniffer_enable(chan, DMA_SNIFF_CTRL_CALC_VALUE_CRC32, true);
    dma_hw->sniff_data = seed;

    // 4. Start the transfer
    // We'll write to a dummy variable to avoid actual memory corruption
    static uint8_t dummy;
    dma_channel_configure(
        chan,
        &c,
        &dummy,     // Write address
        data,       // Read address
        len,        // Element count
        true        // Start immediately
    );

    // 5. Wait for completion
    dma_channel_wait_for_finish_blocking(chan);

    // 6. Get result and cleanup
    uint32_t result = dma_hw->sniff_data;
    dma_sniffer_disable();
    dma_channel_unclaim(chan);

    return result;
}

uint32_t crc32_software(const uint8_t *data, size_t len, uint32_t seed) {
    uint32_t crc = seed;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
