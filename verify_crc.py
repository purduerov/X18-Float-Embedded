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
if os.path.exists(bin_path):
    orig_data = bytearray(open(bin_path, 'rb').read())
    data = bytearray(orig_data)
    size = len(data)
    chunk_size = 220
    padding = (chunk_size - (size % chunk_size)) % chunk_size
    data.extend(b'\xFF' * padding)
    print(f"Size: {len(data)}")
    print(f"CRC:  0x{calculate_crc32(data):08X}")
    print(f"Orig Last 16: {orig_data[-16:].hex(' ')}")
    print(f"Padded Last 16: {data[-16:].hex(' ')}")
else:
    print("File not found")
