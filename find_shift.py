import struct
import os

def calculate_crc32(data):
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return (crc ^ 0xFFFFFFFF) & 0xFFFFFFFF

bin_path = r'.pio\build\float\firmware.bin'
if not os.path.exists(bin_path):
    print("firmware.bin not found")
    exit(1)

with open(bin_path, 'rb') as f:
    orig_data = bytearray(f.read())

CHUNK_SIZE = 220
size = len(orig_data)
padding = (CHUNK_SIZE - (size % CHUNK_SIZE)) % CHUNK_SIZE
padded_data = bytearray(orig_data)
padded_data.extend(b'\xFF' * padding)

total_chunks = len(padded_data) // CHUNK_SIZE
print(f"Total chunks: {total_chunks}")

target_crc = 0xE05D658F

# Scenario 1: One chunk is duplicated, and the last chunk is truncated (to keep size 88000)
for dup_idx in range(total_chunks):
    sim_data = bytearray()
    for i in range(total_chunks):
        if len(sim_data) >= len(padded_data):
            break
        chunk = padded_data[i * CHUNK_SIZE : (i + 1) * CHUNK_SIZE]
        sim_data.extend(chunk)
        if i == dup_idx:
            sim_data.extend(chunk)
    sim_data = sim_data[:len(padded_data)]
    if calculate_crc32(sim_data) == target_crc:
        print(f"[FOUND] Scenario 1: Chunk {dup_idx} (offset {dup_idx * CHUNK_SIZE}) duplicated!")
        
# Scenario 2: One chunk is skipped, and replaced with b'\xFF' at the end (to keep size 88000)
for skip_idx in range(total_chunks):
    sim_data = bytearray()
    for i in range(total_chunks):
        if i == skip_idx:
            continue
        chunk = padded_data[i * CHUNK_SIZE : (i + 1) * CHUNK_SIZE]
        sim_data.extend(chunk)
    sim_data.extend(b'\xFF' * CHUNK_SIZE)
    if calculate_crc32(sim_data) == target_crc:
        print(f"[FOUND] Scenario 2: Chunk {skip_idx} (offset {skip_idx * CHUNK_SIZE}) skipped!")

print("Check finished.")
