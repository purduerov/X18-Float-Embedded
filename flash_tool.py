import serial
import struct
import zlib
import sys
import os
import time

def calculate_crc32(data):
    """Calculates CRC32 exactly as the Pico's crc32_software function does."""
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    # Final XOR 0xFFFFFFFF is standard for CRC32 (zlib)
    return (crc ^ 0xFFFFFFFF) & 0xFFFFFFFF

def read_node_output(ser):
    """Drain any waiting characters from the node and print them."""
    while ser.in_waiting > 0:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line:
            print(f" [Surface] {line}")
            return line
    return None

def stream_firmware(port, bin_path):
    if not os.path.exists(bin_path):
        print(f"Error: File {bin_path} not found.")
        return

    # Read entire file into memory
    with open(bin_path, 'rb') as f:
        firmware_data = bytearray(f.read())

    # Pad to a multiple of 220 bytes (REFLASH_CHUNK_SIZE)
    CHUNK_SIZE = 220
    padding_needed = (CHUNK_SIZE - (len(firmware_data) % CHUNK_SIZE)) % CHUNK_SIZE
    firmware_data.extend(b'\xFF' * padding_needed)

    file_size = len(firmware_data)
    file_crc = calculate_crc32(firmware_data)

    print(f"\n=== HIGH SPEED OTA Reflash Tool ===")
    print(f"File: {bin_path}")
    print(f"Size: {file_size} bytes")
    print(f"CRC:  0x{file_crc:08X}")
    print(f"First 16: {firmware_data[:16].hex(' ')}")
    print(f"Last 16:  {firmware_data[-16:].hex(' ')}")

    try:
        ser = serial.Serial(port, 115200, timeout=0.1)
        time.sleep(1) # Wait for serial bridge to settle
        ser.reset_input_buffer()

        print(f"\n[1/3] Sending Start Command 'S'...")
        # Header: 'S', 4-byte size, 4-byte CRC
        header = b'S' + struct.pack('<I', file_size) + struct.pack('<I', file_crc)
        ser.write(header)
        ser.flush()
        time.sleep(0.2) 

        # Wait for the Transmitter to acknowledge the start
        print("[2/3] Waiting for Radio Sync...")
        sync_start = time.time()
        while True:
            line = read_node_output(ser)
            if line and "ACK received for seq" in line:
                break
            if line and "Failed to start" in line:
                print("Error: Host failed to connect to Float via Radio.")
                return
            if time.time() - sync_start > 15: # 15 second timeout for sync
                print("Error: Timeout waiting for sync. Is the Float on?")
                return
            time.sleep(0.01)

        # Start streaming data
        print("[3/3] Streaming Data...")
        sent_bytes = 0
        while sent_bytes < file_size:
            chunk = firmware_data[sent_bytes:sent_bytes+CHUNK_SIZE]
            
            ser.write(chunk)
            ser.flush()
            
            # Wait for ACK/Progress from node
            chunk_ack = False
            chunk_start = time.time()
            while not chunk_ack:
                line = read_node_output(ser)
                if line and "Progress" in line:
                    chunk_ack = True
                if line and "Link lost" in line:
                    print("\nError: Radio link lost during transfer.")
                    return
                if time.time() - chunk_start > 10: # 10 sec timeout per chunk
                    print(f"\nError: Timeout waiting for ACK on seq {sent_bytes//CHUNK_SIZE}")
                    return
                time.sleep(0.001)
            sent_bytes += CHUNK_SIZE
            # Simple progress bar
            percent = (sent_bytes / file_size) * 100
            sys.stdout.write(f"\r      Progress: {percent:6.1f}% [{sent_bytes}/{file_size} bytes]")
            sys.stdout.flush()

        print("\n\n=== Transfer Complete ===")
        print("Waiting for Float to verify CRC and reboot...")
        
        # Watch for final messages
        final_start = time.time()
        reboot_detected = False
        while time.time() - final_start < 8:
            line = read_node_output(ser)
            if line and "REBOOTING" in line:
                print("\n[SUCCESS] Float is initiating hardware swap and reboot!")
                reboot_detected = True
            if line and "Rebooting Surface" in line:
                print("[SUCCESS] Surface is also resetting.")
                reboot_detected = True
            time.sleep(0.1)
        
        if not reboot_detected:
            print("\nWarning: Transfer finished but reboot confirmation not seen.")
        else:
            print("\nOTA Update Finished Successfully.")

    except serial.SerialException as e:
        print(f"\nSerial Error (likely due to reboot): {e}")
    except KeyboardInterrupt:
        print("\nAborted by user.")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python flash_tool.py <PORT> <BINARY_PATH>")
    else:
        stream_firmware(sys.argv[1], sys.argv[2])
