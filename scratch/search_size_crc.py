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
    crc = binascii.crc32(padded_data[:l]) & 0xffffffff
    if crc == target_crc:
        print(f"[FOUND] Size {l} has CRC {hex(target_crc)}!")
        found = True
        break

if not found:
    # Also check if it's padded with zeros instead of 0xFF
    zero_padded = bytearray(orig_data)
    zero_padded.extend(b'\x00' * padding)
    for l in range(1, len(zero_padded) + 1):
        crc = binascii.crc32(zero_padded[:l]) & 0xffffffff
        if crc == target_crc:
            print(f"[FOUND] Size {l} (zero-padded) has CRC {hex(target_crc)}!")
            found = True
            break

if not found:
    print("No matching size found.")
