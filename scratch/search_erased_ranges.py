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
total_pages = total_size // 256 + 1
target_crc = 0xC3CAD68E

found = False

# Search for any contiguous range of pages [p1, p2] remaining 0xFF
print("Searching for contiguous page ranges remaining 0xFF...")
for p1 in range(total_pages):
    # Skip checking ranges that overlap with 0 (first 16 bytes) or 156 (40k mid)
    # since we know those match the original binary.
    # page 0 offset is 0..255. 
    # 40k offset is page 156 (40000 // 256 = 156).
    if p1 == 0 or p1 == 156:
        continue
    for p2 in range(p1, total_pages):
        if p2 == 156 or p2 == total_pages - 1:
            continue
        sim_data = bytearray(padded_data)
        s = p1 * 256
        e = min((p2 + 1) * 256, total_size)
        sim_data[s:e] = b'\xFF' * (e - s)
        
        if (binascii.crc32(sim_data) & 0xffffffff) == target_crc:
            print(f"[FOUND] Contiguous pages {p1} to {p2} (offsets {s} to {e}) remained 0xFF!")
            found = True
            break
    if found:
        break

# What if a single sector (4KB, i.e. 16 pages) was not written at all?
if not found:
    print("Searching for 4KB sector failures remaining 0xFF...")
    for s_idx in range(total_size // 4096 + 1):
        s = s_idx * 4096
        e = min(s + 4096, total_size)
        # Skip if it overlaps with 0 or 40k or tail
        if s <= 0 < e or s <= 40000 < e:
            continue
        sim_data = bytearray(padded_data)
        sim_data[s:e] = b'\xFF' * (e - s)
        if (binascii.crc32(sim_data) & 0xffffffff) == target_crc:
            print(f"[FOUND] Sector {s_idx} (offset {s} to {e}) remained 0xFF!")
            found = True
            break

# What if a single sector was not erased, and thus became bitwise ANDed with old data?
# We don't have the old data easily, but let's see if we can find other patterns.

if not found:
    print("No contiguous range or sector remaining 0xFF found.")
