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

print(f"Padded size: {len(padded_data)}")
target_crc = 0xC3CAD68E

found = False
for l in range(1, len(padded_data) + 1):
    subdata = padded_data[:l]
    # Standard CRC (seed=0xFFFFFFFF, final XOR)
    crc_std = binascii.crc32(subdata) & 0xffffffff
    # CRC without final XOR (seed=0xFFFFFFFF, no final XOR)
    crc_no_xor = crc_std ^ 0xffffffff
    
    # CRC with seed=0, final XOR
    crc = 0
    for b in subdata:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    crc_seed0_xor = (crc ^ 0xffffffff) & 0xffffffff
    crc_seed0_no_xor = crc
    
    if crc_std == target_crc:
        print(f"[FOUND] Standard CRC, size {l}!")
        found = True
        break
    if crc_no_xor == target_crc:
        print(f"[FOUND] CRC no final XOR, size {l}!")
        found = True
        break
    if crc_seed0_xor == target_crc:
        print(f"[FOUND] Seed 0 with XOR, size {l}!")
        found = True
        break
    if crc_seed0_no_xor == target_crc:
        print(f"[FOUND] Seed 0 no XOR, size {l}!")
        found = True
        break

if not found:
    print("No matching seed/XOR combination found.")
