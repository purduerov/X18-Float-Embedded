import os
import binascii

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

total_size = len(padded_data)
total_chunks = total_size // CHUNK_SIZE
target_crc = 0xC3CAD68E

found = False

# We know that at index 40,000, there is NO shift (matches exactly).
# So any shift or corruption must start AFTER 40,000 (chunk 181).
start_chunk = 182

print(f"Searching for chunk-level shifts after chunk {start_chunk}...")

# 1. Skip one chunk
for skip_idx in range(start_chunk, total_chunks):
    sim_data = padded_data[:skip_idx * CHUNK_SIZE] + padded_data[(skip_idx + 1) * CHUNK_SIZE:] + b'\xFF' * CHUNK_SIZE
    if (binascii.crc32(sim_data) & 0xffffffff) == target_crc:
        print(f"[FOUND] Chunk {skip_idx} (offset {skip_idx * CHUNK_SIZE}) was skipped after 40k!")
        found = True
        break

# 2. Duplicate one chunk
if not found:
    for dup_idx in range(start_chunk, total_chunks):
        sim_data = padded_data[:dup_idx * CHUNK_SIZE] + padded_data[dup_idx * CHUNK_SIZE : (dup_idx + 1) * CHUNK_SIZE] + padded_data[dup_idx * CHUNK_SIZE:]
        sim_data = sim_data[:total_size]
        if (binascii.crc32(sim_data) & 0xffffffff) == target_crc:
            print(f"[FOUND] Chunk {dup_idx} (offset {dup_idx * CHUNK_SIZE}) was duplicated after 40k!")
            found = True
            break

# 3. Arbitrary byte shift after 40k (checking up to 1000 bytes)
if not found:
    print("Searching for arbitrary byte shifts after 40k...")
    for shift in list(range(-1000, 1001)):
        if shift == 0:
            continue
        # Shift can start at any byte offset after 40000
        for start_offset in range(40000, total_size, 4):
            if shift > 0:
                sim_data = padded_data[:start_offset] + b'\xFF' * shift + padded_data[start_offset:]
                sim_data = sim_data[:total_size]
            else:
                abs_shift = abs(shift)
                sim_data = padded_data[:start_offset] + padded_data[start_offset + abs_shift:] + b'\xFF' * abs_shift
            
            if (binascii.crc32(sim_data) & 0xffffffff) == target_crc:
                print(f"[FOUND] Shift of {shift} bytes starting at offset {start_offset}!")
                found = True
                break
        if found:
            break

if not found:
    print("No matching corruption found after 40k.")
