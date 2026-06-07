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

bin_path = '.pio/build/float/firmware.bin'
if os.path.exists(bin_path):
    # Simulate all FF (erased flash)
    corrupted_data = b'\xFF' * 88000
    print(f"All FF CRC: 0x{calculate_crc32(corrupted_data):08X}")
else:
    print("File not found")
