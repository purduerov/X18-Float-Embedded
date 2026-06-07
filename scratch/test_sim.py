import os

bin_path = r'.pio\build\float\firmware.bin'
firmware_data = bytearray(open(bin_path, 'rb').read())
CHUNK_SIZE = 220
padding_needed = (CHUNK_SIZE - (len(firmware_data) % CHUNK_SIZE)) % CHUNK_SIZE
firmware_data.extend(b'\xFF' * padding_needed)

# Simulate target buffering
flash = bytearray(b'\xFF' * len(firmware_data))
page_buffer = bytearray(512)
page_buffer_idx = 0
current_flash_addr = 0

for offset in range(0, len(firmware_data), CHUNK_SIZE):
    chunk = firmware_data[offset : offset + CHUNK_SIZE]
    
    page_buffer[page_buffer_idx : page_buffer_idx + CHUNK_SIZE] = chunk
    page_buffer_idx += CHUNK_SIZE
    
    while page_buffer_idx >= 256:
        flash[current_flash_addr : current_flash_addr + 256] = page_buffer[:256]
        current_flash_addr += 256
        page_buffer_idx -= 256
        if page_buffer_idx > 0:
            # We must copy 256 to 256+page_buffer_idx to 0 to page_buffer_idx
            page_buffer[:page_buffer_idx] = page_buffer[256 : 256 + page_buffer_idx]

# Final flush:
if page_buffer_idx > 0:
    page_buffer[page_buffer_idx:256] = b'\xFF' * (256 - page_buffer_idx)
    flash[current_flash_addr : current_flash_addr + 256] = page_buffer[:256]

# Compare flash to firmware_data
print("Size of flash:", len(flash))
print("Size of firmware_data:", len(firmware_data))
diffs = [i for i in range(len(firmware_data)) if flash[i] != firmware_data[i]]
print("Number of differences:", len(diffs))
if diffs:
    print("First 10 diff offsets:", diffs[:10])
    print("Flash bytes at diff:", [hex(flash[i]) for i in diffs[:10]])
    print("Firmware bytes at diff:", [hex(firmware_data[i]) for i in diffs[:10]])
