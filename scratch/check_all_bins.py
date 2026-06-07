import os
import binascii

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

for root, dirs, files in os.walk(".pio/build"):
    for f in files:
        if f.endswith(".bin"):
            path = os.path.join(root, f)
            with open(path, "rb") as file_obj:
                data = file_obj.read()
            crc = calculate_crc32(data)
            padding = (220 - (len(data) % 220)) % 220
            data_padded = data + b"\xFF" * padding
            padded_crc = calculate_crc32(data_padded)
            print(f"{path}:")
            print(f"  Size: {len(data)} -> Padded: {len(data_padded)}")
            print(f"  CRC: 0x{crc:08X}")
            print(f"  Padded CRC: 0x{padded_crc:08X}")
