#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "hardware/sync.h"
#include "bspatch.h"

// Flash layout offsets
#define APP_OFFSET         0x00008000 // 32KB for Bootloader
#define PATCH_OFFSET       0x00100000 // Slot 1 (Patch file)
#define STAGING_OFFSET     0x00200000 // Slot 2 (New compiled App)

#define OTA_MAGIC          0xDEADBEEF
#define FLASH_SECTOR_SIZE  4096

// Patch stream state
typedef struct {
    uint32_t current_addr;
    uint32_t max_addr;
} patch_stream_state_t;

// Read patch data from Flash (Slot 1)
static int __not_in_flash_func(bootloader_read_patch)(const struct bspatch_stream* stream, void* buffer, int length) {
    if (length < 0) {
        return -1;
    }
    patch_stream_state_t* state = (patch_stream_state_t*)stream->opaque;
    if (state->current_addr + length > state->max_addr) {
        return -1; // EOF or overflow
    }
    
    memcpy(buffer, (const void*)state->current_addr, length);
    state->current_addr += length;
    return 0; // Success
}

// Erase and write the new app into Slot 0 from Slot 2
static void __no_inline_not_in_flash_func(swap_staging_to_app)(uint32_t app_size) {
    uint32_t ints = save_and_disable_interrupts();
    hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);

    uint8_t sector_buffer[FLASH_SECTOR_SIZE];
    for (uint32_t offset = 0; offset < app_size; offset += FLASH_SECTOR_SIZE) {
        const uint8_t *src_ptr = (const uint8_t *)(XIP_BASE + STAGING_OFFSET + offset);
        memcpy(sector_buffer, src_ptr, FLASH_SECTOR_SIZE);

        flash_range_erase(APP_OFFSET + offset, FLASH_SECTOR_SIZE);
        flash_range_program(APP_OFFSET + offset, sector_buffer, FLASH_SECTOR_SIZE);
    }
    
    // Reboot hardware to clear caches and load the newly flashed app
    watchdog_reboot(0, 0, 100);
    while (1) { tight_loop_contents(); }
}

void apply_update() {
    printf("[BOOTLOADER] Applying bspatch OTA Update...\n");

    // Read the patch header to get the new file size
    const uint8_t* patch_base = (const uint8_t*)(XIP_BASE + PATCH_OFFSET);
    
    // Simple check for "ENDSLEY/BSDIFF43" magic in patch header
    if (memcmp(patch_base, "ENDSLEY/BSDIFF43", 16) != 0) {
        printf("[BOOTLOADER] Invalid patch file magic. Aborting.\n");
        return;
    }

    // Read newsize from header+16 (8 bytes, little endian mapped in bspatch logic)
    // The offtin function is static in bspatch.c, so we do a quick local parse for the staging allocation
    int64_t newsize = 0;
    uint8_t* sz_buf = (uint8_t*)(patch_base + 16);
    int64_t y = sz_buf[7] & 0x7F;
    for(int i=6; i>=0; i--) { y = y * 256 + sz_buf[i]; }
    if (sz_buf[7] & 0x80) y = -y;
    newsize = y;

    if (newsize <= 0 || newsize > (1024 * 1024 - APP_OFFSET)) { // Max size is Slot 0 capacity (1MB - APP_OFFSET)
        printf("[BOOTLOADER] Invalid new size: %lld. Aborting.\n", newsize);
        return;
    }

    // Allocate memory for the new app in RAM to do the patching? 
    // No! bspatch writes to RAM, so we need to process it in chunks, OR
    // wait, bspatch(old, oldsize, new, newsize, stream) requires the ENTIRE "new" buffer in RAM!
    // RP2040 has 264KB of RAM. The app is ~150KB. It MIGHT fit.
    // If it doesn't fit, we have to modify bspatch.c to write sequentially to flash.
    
    printf("[BOOTLOADER] Allocating %lld bytes in RAM for new app...\n", newsize);
    uint8_t* new_app_buffer = (uint8_t*)malloc(newsize);
    if (!new_app_buffer) {
        printf("[BOOTLOADER] Failed to allocate RAM for patching. App too large.\n");
        return;
    }

    patch_stream_state_t pstate;
    pstate.current_addr = XIP_BASE + PATCH_OFFSET + 32; // Skip 32-byte header
    pstate.max_addr = XIP_BASE + PATCH_OFFSET + 1024*1024; // 1MB Max patch size

    struct bspatch_stream stream;
    stream.opaque = &pstate;
    stream.read = bootloader_read_patch;

    // We pass the currently active app as "old"
    // Since we don't know the exact old size, we can pass 1MB (the max slot size). bspatch relies on ctrl offsets.
    int result = bspatch((const uint8_t*)(XIP_BASE + APP_OFFSET), 1024*1024, new_app_buffer, newsize, &stream);

    if (result == 0) {
        printf("[BOOTLOADER] Patch successful! Staging to Flash Slot 2...\n");
        // Save to Staging Slot first
        uint32_t ints = save_and_disable_interrupts();
        uint32_t erase_len = (newsize + (FLASH_SECTOR_SIZE - 1)) & ~(FLASH_SECTOR_SIZE - 1);
        flash_range_erase(STAGING_OFFSET, erase_len);
        flash_range_program(STAGING_OFFSET, new_app_buffer, erase_len);
        restore_interrupts(ints);
        free(new_app_buffer);

        printf("[BOOTLOADER] Copying Staging to App Slot and rebooting...\n");
        swap_staging_to_app(erase_len);
    } else {
        printf("[BOOTLOADER] Patch failed with error %d. Booting old app.\n", result);
        free(new_app_buffer);
    }
}

// The Vector Table offset register
#define SCB_VTOR ((volatile uint32_t *)0xe000ed08)

void boot_app() {
    uint32_t app_base = XIP_BASE + APP_OFFSET;
    uint32_t *app_vector_table = (uint32_t *)app_base;

    // Check if the initial stack pointer and reset handler are valid RAM/Flash addresses
    if ((app_vector_table[0] & 0x20000000) == 0 && (app_vector_table[0] & 0x10000000) == 0) {
        printf("[BOOTLOADER] No valid application found at 0x%08lX\n", app_base);
        while(1) tight_loop_contents();
    }

    printf("[BOOTLOADER] Jumping to application at 0x%08lX\n", app_base);
    sleep_ms(10); // allow printf to flush

    // Disable interrupts before handoff
    save_and_disable_interrupts();

    // Set Vector Table Offset Register
    *SCB_VTOR = app_base;

    // Set Stack Pointer
    __asm volatile ("msr msp, %0" : : "r" (app_vector_table[0]));

    // Jump to Reset Handler (entry 1 in the vector table)
    void (*app_reset_handler)(void) = (void (*)(void))(app_vector_table[1]);
    app_reset_handler();
}

int main() {
    stdio_init_all();
    sleep_ms(2000); // Give USB serial time to connect for debugging
    printf("\n\n=== X18-Float Secondary Bootloader ===\n");

    if (watchdog_hw->scratch[0] == OTA_MAGIC) {
        // Acknowledge flag
        watchdog_hw->scratch[0] = 0;
        apply_update();
    } else {
        printf("[BOOTLOADER] Normal boot. No OTA flag detected.\n");
    }

    boot_app();
    return 0;
}