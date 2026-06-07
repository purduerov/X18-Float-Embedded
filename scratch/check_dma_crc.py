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

target_crc = 0xC3CAD68E

# The RP2040 DMA sniffer mode 0: CRC-32 (IEEE 802.3 polynomial).
# Mode 1: CRC-32 reversed.
# Mode 2: CRC-16-CCITT.
# Mode 3: CRC-16-CCITT reversed.
# It also has options for:
# - Bit reverse (reverse the bit order of each input byte)
# - Byte swap (swap the byte order of the 32-bit output)
# - XOR (XOR the output with 0xffffffff)

def reverse_bits(val, bits=8):
    res = 0
    for i in range(bits):
        if val & (1 << i):
            res |= (1 << (bits - 1 - i))
    return res

# Let's try to calculate different variations
def calc_variant(data, bit_rev_in, bit_rev_out, final_xor, byte_swap_out):
    crc = 0xFFFFFFFF
    for b in data:
        if bit_rev_in:
            b = reverse_bits(b)
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    
    # standard crc has been computed. Now apply output transformations
    if bit_rev_out:
        # bit reverse 32-bit
        crc = reverse_bits(crc, 32)
    
    if final_xor:
        crc ^= 0xFFFFFFFF
        
    if byte_swap_out:
        # Swap bytes: A B C D -> D C B A
        crc = ((crc & 0xff) << 24) | ((crc & 0xff00) << 8) | ((crc & 0xff0000) >> 8) | ((crc >> 24) & 0xff)
        
    return crc & 0xffffffff

found = False
for bit_rev_in in [False, True]:
    for bit_rev_out in [False, True]:
        for final_xor in [False, True]:
            for byte_swap_out in [False, True]:
                crc = calc_variant(padded_data, bit_rev_in, bit_rev_out, final_xor, byte_swap_out)
                if crc == target_crc:
                    print(f"[FOUND] DMA Sniffer Configuration:")
                    print(f"  bit_rev_in: {bit_rev_in}")
                    print(f"  bit_rev_out: {bit_rev_out}")
                    print(f"  final_xor: {final_xor}")
                    print(f"  byte_swap_out: {byte_swap_out}")
                    found = True
                    break
            if found:
                break
        if found:
            break
    if found:
        break

if not found:
    print("No matching DMA configuration found.")
